#ifndef BITCOIN_QT_CONFIGWIDGET_H
#define BITCOIN_QT_CONFIGWIDGET_H

#include <QDialog>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QAbstractButton;

namespace Ui {
    class ConfigDialog;
}

class ConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConfigDialog(QDialog *parent = nullptr);
    ~ConfigDialog();

    int exec() override;
    void accept() override;
    QString selectedPool();

private:
    void updateList();
    void getPolls();

private:
    Ui::ConfigDialog *ui;

    QStringList poolList;
    QNetworkAccessManager *mgr;

private Q_SLOTS:
    void handleResponse(QNetworkReply *reply);
    void retryPressed(QAbstractButton *button);
    void okPressed(QAbstractButton *button);
    void canclePressed(QAbstractButton *button);
    void buttonPressed(QAbstractButton *button);
};
#endif
