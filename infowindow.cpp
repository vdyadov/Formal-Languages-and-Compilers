#include "infowindow.h"

#include <QUrl>

InfoWindow::InfoWindow(const QString &title, const QString &resourcePath, QWidget *parent)
    : QDialog(parent)
{
    setupUi();

    setWindowTitle(title);

    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString markdown = QString::fromUtf8(file.readAll());
        file.close();
        // Чтобы в Markdown работали относительные пути к картинкам из qrc (например в TEXT_GRAMMAR.md)
        m_browser->document()->setBaseUrl(QUrl(QStringLiteral("qrc:/")));
        m_browser->setMarkdown(markdown);
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
