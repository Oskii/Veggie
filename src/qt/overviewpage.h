// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

#include "amount.h"
#include "configdialog.h"

#include <QWidget>
#include <memory>
#include <QProcess>
#include <QDateTime>
#include <QTextStream>

class ClientModel;
class TransactionFilterProxy;
class TxViewDelegate;
class PlatformStyle;
class WalletModel;
//class QWebEngineView;
class TransactionTableModel;

class QThread;

class QFile;
namespace Ui {
    class OverviewPage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

private:

    /**
     * @brief OverviewPage::fillTransactionInformation
     * Fill transaction table and table with inforamtion about summaries raised for animals with required information.
     * First column - icon, which describe direction of the transaction,
     * Second - Date of the transaction
     * Third - Amount of the transaction
     * @param transactionTableModel
     * @param isAnimalFunds - depends on this, set data to transaction or animal funds table
     */
    void fillTransactionInformation(TransactionTableModel * const transactionTableModel, bool isAnimalFunds = false);
    bool isWalletValid();
    void setWalletInvalid(bool);
    void showWarning(QString message);
    void startMining();
    void updateRank();

public Q_SLOTS:
    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                    const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

Q_SIGNALS:
    void transactionClicked(const QModelIndex &index);
    void outOfSyncWarningClicked();

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    CAmount currentBalance;
    CAmount currentUnconfirmedBalance;
    CAmount currentImmatureBalance;
    CAmount currentWatchOnlyBalance;
    CAmount currentWatchUnconfBalance;
    CAmount currentWatchImmatureBalance;

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;
    QString poolComand;
    QProcess *process;
    ConfigDialog configDialog;
    QDateTime latestMiningOutputDate;

//    QWebEngineView *webView;

    QString poolUrlString;

    QString ccminerName;

    //thread used to executing ccminer.exe in another thread
    QThread *processThread;

    //log file, temporary
    QFile *logFile;
    QTextStream textStream;

    static constexpr double raisedForAnimalsMultiplier = 0.25;
private Q_SLOTS:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateWatchOnlyLabels(bool showWatchOnly);
    void handleOutOfSyncWarningClicks();

    /**
     * @brief OverviewPage::startMiningSlot
     * start or stop maining depends on current state.
     * Activates after pressing "Start maining/Stop Maining button"
     */
    void startMiningSlot();
    void showConfig();
    void walletTextChanged(const QString &arg1);
    bool fileExists(QString path);
    void miningStarted();
    void miningErrorOccurred(QProcess::ProcessError);

    /**
     * @brief OverviewPage::mainingResultOutput
     * Insert information about maining into console log
     */
    void mainingResultOutput();
};

#endif // BITCOIN_QT_OVERVIEWPAGE_H
