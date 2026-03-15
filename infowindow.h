#ifndef INFOWINDOW_H
#define INFOWINDOW_H

#include <QDialog>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QFile>

class InfoWindow : public QDialog {
    Q_OBJECT

public:
    // Конструктор принимает все, что нужно для настройки "личности" окна
    explicit InfoWindow(const QString &title, const QString &resourcePath, QWidget *parent = nullptr);

private:
    QTextBrowser *m_browser;

    void setupUi();
};

#endif // INFOWINDOW_H
