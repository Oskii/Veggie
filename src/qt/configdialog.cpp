#include "configdialog.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QLIstWidget>
#include <QListWidgetItem>

#define POOL_URL "https://www.veggiecoin.io/pools.list"

ConfigDialog::ConfigDialog(QDialog parent) : QDialog(parent),
    ui(new Ui::ConfigDialog),
    mgr{new QNetworkAccessManager}
{
  ui->setupUi(this);

  connect(mgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleResponse(QNetworkReply*)));
}

ConfigDialog::~ConfigDialog()
{
    delete mgr;
}

void ConfigDialog::show()
{
    QDialog::show();

    mgr->get(QNetworkRequest(QUrl(POOL_URL)));
}

QString ConfigDialog::selectedPool()
{
    return poolList.at(ui->listWidgetPool.currentRaw());
}

void ConfigDialog::updateList()
{
    ui->listWidgetPool.clear();

    foreach (QString itemText, poolList) {
        QListWidgetItem item(itemText);
        ui->listWidgetPool.addItem(item);
    }
}

void ConfigDialog::handleResponse(QNetworkReply *reply)
{
    QByteArray responceData = reply->readAll();

    poolList = responceData.split("\n");
}
