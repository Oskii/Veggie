#ifndef BITCOIN_QT_CONFIGWIDGET_H
#define BITCOIN_QT_CONFIGWIDGET_H

#include <QWidget>

class QNetworkAccessManager;
class QNetworkReply;

namespace Ui {
    class ConfigDialog;
}

class ConfigDialog : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigDialog(QDialog parent = nullptr);
    ~ConfigDialog();

    void show();
    QString selectedPool();

private:
    void updateList();

private:
    Ui::ConfigDialog *ui;

    QStringList poolList;
    QNetworkAccessManager *mgr;

private Q_SLOTS:
    void handleResponse(QNetworkReply *reply);
};
#endif
