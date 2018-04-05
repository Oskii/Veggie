#include "configdialog.h"
#include "ui_configdialog.h"

#include <iostream>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QTimer>

#define POOL_URL "https://www.veggiecoin.io/pools.list"
#define RETRY_TIME  5000

ConfigDialog::ConfigDialog(QDialog *parent) : QDialog(parent),
    ui(new Ui::ConfigDialog),
    mgr{new QNetworkAccessManager()},
    retryTimer{new QTimer(this)}
{
  ui->setupUi(this);

  connect(mgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleResponse(QNetworkReply*)));
  connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
  connect(retryTimer, SIGNAL(timeout()), this, SLOT(allowRetry()));
}

ConfigDialog::~ConfigDialog()
{
    delete retryTimer;
    delete mgr;
    delete ui;
}

int ConfigDialog::exec()
{
    getPolls();

    return QDialog::exec();
}

void ConfigDialog::accept()
{

}

QString ConfigDialog::selectedPool()
{
    return poolList.at(ui->listWidgetPool->currentRow());
}

void ConfigDialog::showMessage(QString m)
{
    ui->labelMessage->setText(m);
}

void ConfigDialog::updateList()
{
    QString itemText;
    for (int i=0; i<poolList.count();i++) {
        itemText = poolList.at(i);
        if (!itemText.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(itemText);
            ui->listWidgetPool->addItem(item);
        }
    }

    if (poolList.count() > 0) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        ui->listWidgetPool->setCurrentRow(0);
    } else {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void ConfigDialog::getPolls()
{
    isRetryAllowed = false;
    retryTimer->start(RETRY_TIME);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->listWidgetPool->clear();
    showMessage(tr("Please wait while loading available pools"));

    mgr->get(QNetworkRequest(QUrl(POOL_URL)));
}

void ConfigDialog::handleResponse(QNetworkReply *reply)
{
    QByteArray responceData = reply->readAll();
    QString respons(responceData);

    poolList.clear();
    poolList = respons.split("\n");
    showMessage(tr("Pool list received"));

    updateList();
}

void ConfigDialog::retryPressed(QAbstractButton *)
{
    if (isRetryAllowed){
        getPolls();
    } else {
        showMessage(tr("Too many requests per seconds. Please wait 5 seconds."));
    }
}

void ConfigDialog::okPressed(QAbstractButton *)
{
    if (ui->listWidgetPool->currentRow() >= 0) {
        QDialog::accept();
    }
}

void ConfigDialog::canclePressed(QAbstractButton *)
{
    QDialog::reject();
}

void ConfigDialog::buttonPressed(QAbstractButton *button)
{
    if (button == ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okPressed(button);
    } else if (button == ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        canclePressed(button);
    } else if (button == ui->buttonBox->button(QDialogButtonBox::Retry)) {
        retryPressed(button);
    }
}

void ConfigDialog::allowRetry()
{
    isRetryAllowed = true;
    showMessage(tr(""));
}
