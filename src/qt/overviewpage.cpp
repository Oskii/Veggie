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

#include <QWebView>
#include <QUrl>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

#define WALLET_ADDR_KEY "WALLETADDRESS"
#define BAT_FILE        "./comand.bat"

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

    setWalletInvalid(true);

    webView = new QWebView(ui->webWidget);

    connect(process, SIGNAL(started()), this, SLOT(miningStarted()));
    connect(process, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(miningErrorOccurred(QProcess::ProcessError)));

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    //raised for animals
    ui->listRaisedForAnimals->setItemDelegate(txdelegate);
    ui->listRaisedForAnimals->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listRaisedForAnimals->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listRaisedForAnimals->setAttribute(Qt::WA_MacShowFocusRect, false);

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    //connecting mining buttons
    connect(ui->pushButtonStartMining, SIGNAL(pressed()), this, SLOT(startMiningSlot()));
    connect(ui->pushButtonConfig, SIGNAL(pressed()), this, SLOT(showConfig()));
    connect(ui->lineEditWalletAddress, SIGNAL(textChanged(const QString)), this, SLOT(walletTextChanged(const QString)));

    ui->lineEditConfig->setStyleSheet("border: 1px solid gray; color: gray; background-color: white;");

    miningOutput = tr("There no logs yet");
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
    if (fileExists(BAT_FILE)) {
        if (!fileExists(poolComand.split(" ").first())) {
            showWarning(tr("It looks like ccminer in not in the same folder with your binary"));
        } else {
            QFileInfo cmdFile( "C:\\Windows\\system32\\cmd.exe");
            QStringList l;
            l << "/c";
            l << QDir::currentPath() + BAT_FILE;
            process->start(cmdFile.absoluteFilePath(), l);
        }
    } else {
        showWarning(tr("Something went wrong while trying to exec *.bat file"));
    }
#else
    showWarning(tr("At the moment Windows OS is supported only"));
#endif
}

void OverviewPage::updateRank()
{
    double veggie = ui->labelTotal->text().split(" ").at(0).toDouble();
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
    delete webView;
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
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        ui->listRaisedForAnimals->setModel(filter.get());
        ui->listRaisedForAnimals->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("VEGI")
    updateDisplayUnit();
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

        ui->listTransactions->update();
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
            ui->pushButtonStartMining->setText(MINING_START);
        }
    } else {
        setWalletInvalid(isWalletValid());

        if (poolComand.isEmpty()) {
            ui->lineEditConfig->setStyleSheet("border: 1px solid red; color: gray; background-color: white;");
            showWarning(tr("Please select config"));
        } else if (isWalletValid()) {
            if (poolComand.contains(WALLET_ADDR_KEY)) {
                poolComand.replace(WALLET_ADDR_KEY, ui->lineEditWalletAddress->text());

                QFile file(BAT_FILE);
                if (file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) {
                    QTextStream in(&file);
                    in << poolComand;
                    file.flush();
                    file.close();

                    startMining();
                } else {
                    showWarning(tr("Something went wrong while trying to overwrite *.bat file "));
                }
            }
        }
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
            webView->load(QUrl(urlString));
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

void OverviewPage::readyReadStandardOutput()
{
    miningOutput = process->readAll();
    latestMiningOutputDate = QDateTime::currentDateTime();

    ui->logView->append(miningOutput);
}
