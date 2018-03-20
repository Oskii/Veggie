// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#include <QProcess>
#include <QNetworkAccessManager>
#include <QListWidget>
#include <QLayout>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>

#include <QTextStream>

//#include <QWebEngineView>
#include <QUrl>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

#define WALLET_ADDR_KEY "WALLETADDRESS"

#define MINING_START    "Start Mining"
#define MINING_STOP     "Stop Mining"

#define potato(v)   (v > 0) && (v<99.99999999)
#define carrot(v)   (v > 100) && (v<249.99999999)
#define earcorn(v)  (v > 250) && (v<499.99999999)
#define avocado(v)  (v > 500) && (v<999.99999999)
#define brocoly(v)  (v > 1000) && (v<1999.99999999)
#define eggplant(v) (v > 2000) && (v<4999.99999999)
#define cucumber(v) (v > 5000) && (v<9999.99999999)
#define mushroom(v) (v > 10000) && (v<14999.99999999)
#define pig_face(v) (v > 15000) && (v<19999.99999999)
#define fox_faxe(v) (v > 20000) && (v<29999.99999999)
#define lion_face(v) (v > 30000) && (v<49999.99999999)
#define cat_face(v) (v > 50000) && (v<74999.99999999)
#define god_face(v) (v > 7500)


class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::VEGI),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    txdelegate(new TxViewDelegate(platformStyle, this)),
    process{new QProcess()}
{
    ui->setupUi(this);

    this->setWindowTitle(VEGGIE_PACKAGE_NAME);

    setWalletInvalid(true);

//    webView = new QWebEngineView(ui->webWidget);

    connect(process, SIGNAL(started()), this, SLOT(miningStarted()));
    connect(process, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(miningErrorOccurred(QProcess::ProcessError)));

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);


    ui->tableTransactions->setStyleSheet("color: black;"
                                         "background-color: transparent;"
                                         "selection-color: white;"
                                         "selection-background-color: grey;"
                                         "border:none;");


    ui->tableTransactions->verticalHeader()->hide();
    ui->tableTransactions->setShowGrid(false);
    ui->tableTransactions->horizontalHeader()->setVisible(false);


    ui->tableRaisedForAnimals->setStyleSheet("color: black;"
                                             "background-color: transparent;"
                                             "selection-color: white;"
                                             "selection-background-color: grey;"
                                             "border: none");

    ui->tableRaisedForAnimals->verticalHeader()->hide();
    ui->tableRaisedForAnimals->setShowGrid(false);
    ui->tableRaisedForAnimals->horizontalHeader()->setVisible(false);


    connect(ui->tableTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    //connecting mining buttons
    connect(ui->pushButtonStartMining, SIGNAL(pressed()), this, SLOT(startMiningSlot()));
    connect(ui->pushButtonConfig, SIGNAL(pressed()), this, SLOT(showConfig()));
    connect(ui->lineEditWalletAddress, SIGNAL(textChanged(const QString)), this, SLOT(walletTextChanged(const QString)));

    ui->lineEditConfig->setStyleSheet("border: 1px solid gray; color: gray; background-color: white;");

}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

void OverviewPage::startMining()
{
#ifdef Q_OS_WIN
            QString fileName = QDir::currentPath() + "//ccminer-x64.exe";
            if (QFileInfo::exists(fileName)) {
                if (poolComand.contains(WALLET_ADDR_KEY)) {
                    poolComand.replace(WALLET_ADDR_KEY, ui->lineEditWalletAddress->text());

                    if (process->state() == QProcess::Running) {
                        qDebug() << "Killing old process...";
                        process->close();
                        process->kill();
                    }

                    QStringList commandsList = poolComand.split(" ");
                    QStringList arguments;
                    arguments << "/C";
                    QString program("C:/windows/system32/cmd.exe");

                    for(auto item: commandsList) {
                        arguments << item;
                    }

                    process->start(program, arguments);
                    process->waitForFinished();
                    } else {
                        showWarning(tr("CCMiner is not exists!"));
                    }
                disconnect(process, SIGNAL(readyRead()), this, SLOT(mainingResultOutput()));
                connect(process, SIGNAL(readyRead()), this, SLOT(mainingResultOutput()));
            } else {
                showWarning(tr("File wasn't found!"));
            }
#else
    showWarning(tr("At the moment Windows OS is supported only"));
#endif
}

void OverviewPage::updateRank()
{
//    double veggie = ui->labelTotal->text().split(" ").at(0).toDouble();

    double veggie = ui->totalRaisedForAnimalsValue->text().split(" ").at(0).toDouble();

    QLabel *rank = ui->labelRankLogo;

    if (potato(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/potato"));
    } else if (carrot(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/carrot"));
    } else if (earcorn(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/cat-face"));
    } else if (avocado(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/avocado"));
    } else if (brocoly(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/broccoli"));
    } else if (eggplant(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/eggplant"));
    } else if (cucumber(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/cucumber"));
    } else if (mushroom(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/mushroom"));
    } else if (pig_face(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/pig-face"));
    } else if (fox_faxe(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/fox-face"));
    } else if (lion_face(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/lion-face"));
    } else if (cat_face(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/cat-face"));
    } else if (god_face(veggie)) {
        rank->setPixmap(QPixmap(":/emoji/dog-face"));
    }
}

OverviewPage::~OverviewPage()
{
//    delete webView;
    if (process->state() == QProcess::Running) {
    	process->close();
        process->kill();
    }
    QStringList listOfCommands;
    listOfCommands << "/C" << "taskkill" << "/IM" << "ccminer-x64.exe" << "/F";
    QProcess::execute(QString("C:/windows/system32/cmd.exe"), listOfCommands);

    delete process;
    delete ui;
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchAvailable->setText(BitcoinUnits::formatWithUnit(unit, watchOnlyBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::formatWithUnit(unit, watchUnconfBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::formatWithUnit(unit, watchImmatureBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::formatWithUnit(unit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance, false, BitcoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    bool showWatchOnlyImmature = watchImmatureBalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance

    updateRank();
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}


void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        TransactionTableModel *transactionTableModel = model->getTransactionTableModel();

        //recent transactions
        fillTransactionInformation(transactionTableModel);
        //veggie raised for animal
        fillTransactionInformation(transactionTableModel, true);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());

        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)),
                this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("VEGI")
    updateDisplayUnit();
}


void OverviewPage::fillTransactionInformation(TransactionTableModel * const transactionTableModel, bool isAnimalFunds)
{
    QTableView *currentTableView = nullptr;
    float amountMultiplier = 1.0f;

    if (isAnimalFunds) {
        amountMultiplier *= raisedForAnimalsMultiplier;
        currentTableView = ui->tableRaisedForAnimals;
    } else {
        currentTableView = ui->tableTransactions;
    }

    int currentRow = 0;
    float totalValue = 0.0f;

    QStandardItemModel* modelStandard = new QStandardItemModel(0);
    modelStandard->setHorizontalHeaderItem(0, new QStandardItem(""));
    modelStandard->setHorizontalHeaderItem(1, new QStandardItem("Date"));
    modelStandard->setHorizontalHeaderItem(2, new QStandardItem("Amount"));

    QModelIndex topLeft = transactionTableModel->index(0,0);

    for (int i(0); i < transactionTableModel->rowCount(topLeft); ++i) {
        QModelIndex modelIndex = transactionTableModel->index(i, 4);//4 - for icon

        //get data from model
        QIcon iconByIndex = qvariant_cast<QIcon>(modelIndex.data(TransactionTableModel::WatchonlyDecorationRole));
        //QDateTime dateTimeByIndex = modelIndex.data(TransactionTableModel::DateRole).toDateTime();
        QString dateTimeByIndex = modelIndex.data(TransactionTableModel::DateRole).toString();
        float amountByIndex = modelIndex.data(TransactionTableModel::AmountRole).toFloat() / 100000000.0f;

        float currentValue = amountByIndex * amountMultiplier;
        QString stringMulptilpliedValue;
        stringMulptilpliedValue.setNum(currentValue, 'f');

        if (currentValue >= 0.0f) {
            stringMulptilpliedValue = "+" + stringMulptilpliedValue;
            totalValue += currentValue;
        } else {
            //amount for animal can't be negative
            if (isAnimalFunds) {
                continue;
            }
        }
        stringMulptilpliedValue += " VEGI";

        QStandardItem *amount = new QStandardItem(stringMulptilpliedValue);
        QStandardItem *date = new QStandardItem(dateTimeByIndex);
        QStandardItem *icon = new QStandardItem();
        icon->setData(QVariant(modelIndex.data(TransactionTableModel::RawDecorationRole)), Qt::DecorationRole);

        if (currentValue < 0.0f) {
            amount->setForeground(QBrush(Qt::red));
        }
        modelStandard->setItem(currentRow, 0, icon);
        modelStandard->setItem(currentRow, 1, date);
        modelStandard->setItem(currentRow, 2, amount);

        ++currentRow;
    }

    // Set up transaction list
//    filter.reset(new TransactionFilterProxy());
//    filter->setSourceModel(modelStandard);
//    filter->setLimit(NUM_ITEMS);
//    filter->setDynamicSortFilter(true);
//    filter->setSortRole(Qt::EditRole);
//    filter->setShowInactive(false);
//    filter->sort(TransactionTableModel::DateRole, Qt::DescendingOrder);

    currentTableView->setModel(modelStandard);
    currentTableView->resizeRowsToContents();
    currentTableView->resizeColumnsToContents();

    //set raised for animals total, if neeeded
    if (isAnimalFunds) {
        QString totalMultipliedValue;
        totalMultipliedValue.setNum(totalValue, 'f');
        totalMultipliedValue += " VEGI";
        ui->totalRaisedForAnimalsValue->setText(totalMultipliedValue);
    }
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->tableTransactions->update();
        //ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

bool OverviewPage::isWalletValid()
{
    return !ui->lineEditWalletAddress->text().isEmpty();
}

void OverviewPage::setWalletInvalid(bool isValid)
{
    if (isValid) {
        showWarning("");
        ui->lineEditWalletAddress->setStyleSheet("border: 1px solid gray");
    } else {
        showWarning(tr("Please enter your wallet"));
        ui->lineEditWalletAddress->setStyleSheet("border: 1px solid red");
    }
}

void OverviewPage::showWarning(QString message)
{
    ui->labelMessage->setText(message);
}

void OverviewPage::startMiningSlot()
{
    if (ui->pushButtonStartMining->text() == MINING_STOP) {
        if (process != nullptr) {
            process->close();
            showWarning(tr("Mining successfully stoped!"));
	    ui->logView->append("Stoped mining");
            ui->pushButtonStartMining->setText(MINING_START);
        }
    } else {
        setWalletInvalid(isWalletValid());

        if (poolComand.isEmpty()) {
            ui->lineEditConfig->setStyleSheet("border: 1px solid red; color: gray; background-color: white;");
            showWarning(tr("Please select config"));
        } else if (isWalletValid()) {
            startMining();
        }
    }
}

void OverviewPage::mainingResultOutput()
{
    qDebug() << "Process output:";
    QByteArray output = process->readAllStandardOutput();
    QByteArray error = process->readAllStandardError();
   // process->close();

    latestMiningOutputDate = QDateTime::currentDateTime();
    QFile outLogFile("log.txt");
    outLogFile.open(QIODevice::WriteOnly | QIODevice::Append);


    QTextStream outTextStream(&outLogFile);
    outTextStream << output << endl;
    outTextStream << error << endl;
    
    outLogFile.flush();
    outLogFile.close();
    if (!output.isEmpty()) {
	ui->logView->append(output);
        qDebug() << output;

    }
    if (!error.isEmpty()) {
	 ui->logView->append(error);
         qDebug() << "Error Message:";
         qDebug() << error;
    }
}

void OverviewPage::showConfig()
{
    if (configDialog.exec() == QDialog::Accepted) {
        poolComand = configDialog.selectedPool();
        showWarning("");

        QStringList list = poolComand.split(" ");
        QStringList urlList = list.at(4).split(":");

        if (urlList.count() == 3) {
            QString lTmp = urlList.at(1);
            QString urlString;
            urlString.append("http://").append(lTmp.remove(0, 2));
//            webView->load(QUrl(urlString));
//            webView->show();
        }

        ui->lineEditConfig->setText(poolComand);
        ui->lineEditConfig->setStyleSheet("border: 1px solid gray; color: black; background-color: white;");
    }
}

void OverviewPage::walletTextChanged(const QString &arg1)
{
    setWalletInvalid(isWalletValid());
}

bool OverviewPage::fileExists(QString path) {
    QFileInfo file(path);
    // check if file exists and if yes: Is it really a file and no directory?
    if (file.exists() && file.isFile()) {
        return true;
    } else {
        return false;
    }
}

void OverviewPage::miningStarted()
{
    showWarning(tr("Mining successfully started!"));
    ui->pushButtonStartMining->setText(MINING_STOP);
}

void OverviewPage::miningErrorOccurred(QProcess::ProcessError err)
{
    switch(err) {
    case QProcess::FailedToStart:
        showWarning(tr("Script file not found, resource error"));
        break;
    case QProcess::Crashed:
        showWarning(tr("Ccminer crashed"));
        break;
    case QProcess::Timedout:
        showWarning(tr("Ccminer timedout"));
        break;
    case QProcess::ReadError:
        showWarning(tr("Read error"));
        break;
    case QProcess::WriteError:
        showWarning(tr("Write error"));
        break;
    case QProcess::UnknownError:
        showWarning(tr("Unknown error"));
        break;
    }

    ui->pushButtonStartMining->setText("Start Mining");
}
