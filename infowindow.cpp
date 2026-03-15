#include "infowindow.h"

InfoWindow::InfoWindow(const QString &title, const QString &resourcePath, QWidget *parent)
    : QDialog(parent)
{
    setupUi();

    setWindowTitle(title);

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_browser->setMarkdown(file.readAll());
        file.close();
    } else {
        m_browser->setText("Ошибка: Не удалось загрузить файл " + resourcePath);
    }
}

void InfoWindow::setupUi() {
    resize(600, 450);
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_browser = new QTextBrowser(this);

    m_browser->setOpenExternalLinks(true);

    layout->addWidget(m_browser);
    setLayout(layout);
}
