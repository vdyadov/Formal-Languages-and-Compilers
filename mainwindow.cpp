#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- Файл ---
    ui->action_new->setShortcut(QKeySequence::New);       // Ctrl+N
    ui->action_open->setShortcut(QKeySequence::Open);     // Ctrl+O
    ui->action_save->setShortcut(QKeySequence::Save);     // Ctrl+S
    ui->action_exit->setShortcut(QKeySequence(Qt::Key_F10)); // Часто в лабах просят F10 для выхода

    // --- Правка ---
    ui->action_undo->setShortcut(QKeySequence::Undo);     // Ctrl+Z
    ui->action_redo->setShortcut(QKeySequence::Redo);     // Ctrl+Y
    ui->action_cut->setShortcut(QKeySequence::Cut);       // Ctrl+X
    ui->action_copy->setShortcut(QKeySequence::Copy);     // Ctrl+C
    ui->action_paste->setShortcut(QKeySequence::Paste);   // Ctrl+V
    ui->action_delete->setShortcut(QKeySequence(Qt::Key_Delete)); // Клавиша Del

    // --- Зум (Масштаб) ---
    // ui->action_zoom_in->setShortcut(QKeySequence::ZoomIn);   // Ctrl++
    // ui->action_zoom_out->setShortcut(QKeySequence::ZoomOut); // Ctrl+-

    // --- Пуск (Компиляция) ---
    ui->action_run->setShortcut(QKeySequence(Qt::Key_F5));   // Стандарт для запуска кода

    // connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    // connect(ui->action_new, &QAction::triggered, this, &MainWindow::on_action_new_triggered);

    // connect(ui->action_copy, &QAction::triggered, this, &MainWindow::on_action_copy_triggered);
    // connect(ui->action_cut, &QAction::triggered, this, &MainWindow::on_action_cut_triggered);
    // connect(ui->action_paste, &QAction::triggered, this, &MainWindow::on_action_paste_triggered);
    // connect(ui->action_undo, &QAction::triggered, this, &MainWindow::on_action_undo_triggered);
    // connect(ui->action_redo, &QAction::triggered, this, &MainWindow::on_action_redo_triggered);
    // connect(ui->action_delete, &QAction::triggered, this, &MainWindow::on_action_delete_triggered);
    // connect(ui->action_select_all, &QAction::triggered, this, &MainWindow::on_action_select_all_triggered);

    connect(ui->action_run, &QAction::triggered, this, &MainWindow::runCompiler);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, [this](int index) {
        QWidget* tab = ui->tabWidget->widget(index);
        ui->tabWidget->removeTab(index);
        delete tab; // Важно освободить память
    });
}

void MainWindow::runCompiler() {
    if (CodeEditor *editor = currentEditor()) {
        QString code = editor->toPlainText();
    }
    ui->statusbar->showMessage("Анализ запущен...", 2000);
}

CodeEditor* MainWindow::currentEditor() {
    return qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
}

// Настройка новой вкладки
void MainWindow::setupEditor(CodeEditor *editor, const QString &fullPath) {
    QString title = fullPath.isEmpty() ? "Untitled" : QFileInfo(fullPath).fileName();
    if (!fullPath.isEmpty()) editor->setProperty("filePath", fullPath);

    int index = ui->tabWidget->addTab(editor, title);
    ui->tabWidget->setCurrentIndex(index);

    // Соединяем сигнал изменения текста со "звездочкой" в заголовке
    connect(editor->document(), &QTextDocument::modificationChanged, this, &MainWindow::updateTabTitle);
}

void MainWindow::updateTabTitle(bool modified) {
    int idx = ui->tabWidget->currentIndex();
    QString title = ui->tabWidget->tabText(idx);
    if (modified && !title.endsWith("*")) ui->tabWidget->setTabText(idx, title + "*");
    else if (!modified && title.endsWith("*")) ui->tabWidget->setTabText(idx, title.left(title.length()-1));
}

// Пример для кнопки "Создать"
void MainWindow::on_action_new_triggered() {
    CodeEditor *editor = new CodeEditor();
    ui->tabWidget->addTab(editor, "new.cpp");
    ui->tabWidget->setCurrentWidget(editor);
}

// // СОЗДАТЬ
// void MainWindow::on_action_new_triggered() {
//     setupEditor(new CodeEditor(), "");
// }

// ОТКРЫТЬ
void MainWindow::on_action_open_triggered() {
    QString path = QFileDialog::getOpenFileName(this, "Открыть файл", "", "C++ Files (*.cpp *.h);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        CodeEditor *editor = new CodeEditor();
        editor->setPlainText(file.readAll());
        file.close();
        setupEditor(editor, path);
        editor->document()->setModified(false); // Сбрасываем флаг после загрузки
    }
}

// СОХРАНИТЬ (Вспомогательный метод)
bool MainWindow::saveFile(CodeEditor *editor, const QString &path) {
    QString actualPath = path.isEmpty() ? editor->property("filePath").toString() : path;

    if (actualPath.isEmpty()) return false; // Нужно вызвать "Сохранить как"

    QFile file(actualPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << editor->toPlainText();
        file.close();
        editor->setProperty("filePath", actualPath);
        editor->document()->setModified(false);
        return true;
    }
    return false;
}

void MainWindow::on_action_save_triggered() {
    if (CodeEditor *ed = currentEditor()) {
        if (ed->property("filePath").toString().isEmpty()) on_action_save_as_triggered();
        else saveFile(ed);
    }
}

void MainWindow::on_action_save_as_triggered() {
    if (CodeEditor *ed = currentEditor()) {
        QString path = QFileDialog::getSaveFileName(this, "Сохранить как", "", "C++ Files (*.cpp);;All Files (*)");
        if (!path.isEmpty() && saveFile(ed, path)) {
            ui->tabWidget->setTabText(ui->tabWidget->currentIndex(), QFileInfo(path).fileName());
        }
    }
}

bool MainWindow::maybeSave(CodeEditor *editor) {
    if (!editor || !editor->document()->isModified()) return true;

    auto ret = QMessageBox::warning(this, "Compiler", "Файл изменен. Сохранить?",
                                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) {
        on_action_save_triggered();
        return !editor->document()->isModified();
    }
    return (ret == QMessageBox::Discard);
}

void MainWindow::closeTab(int index) {
    CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(index));
    if (maybeSave(ed)) {
        ui->tabWidget->removeTab(index);
        delete ed;
    }
}

void MainWindow::on_action_exit_triggered() {
    close(); // Вызывает closeEvent
}

void MainWindow::closeEvent(QCloseEvent *event) {
    for (int i = ui->tabWidget->count()-1; i >= 0; --i) {
        ui->tabWidget->setCurrentIndex(i);
        if (!maybeSave(currentEditor())) {
            event->ignore();
            return;
        }
        ui->tabWidget->removeTab(i);
    }
    event->accept();
}

// Пример для кнопки "Копировать"
void MainWindow::on_action_copy_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->copy();
    }
}

void MainWindow::on_action_cut_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->cut();
    }
}

void MainWindow::on_action_paste_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->paste();
    }
}

void MainWindow::on_action_undo_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->undo();
    }
}

void MainWindow::on_action_redo_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->redo();
    }
}

void MainWindow::on_action_delete_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->textCursor().deleteChar();
    }
}

void MainWindow::on_action_select_all_triggered() {
    if (CodeEditor *editor = currentEditor()) {
        editor->selectAll();
    }
}

#include <QDialog>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QFile>

void MainWindow::on_action_about_triggered() {
    // 1. Создаем диалоговое окно
    QDialog *aboutDialog = new QDialog(this);
    aboutDialog->setWindowTitle("О программе");
    aboutDialog->setMinimumSize(600, 450); // Устанавливаем комфортный размер

    // 2. Создаем вертикальный лейаут
    QVBoxLayout *layout = new QVBoxLayout(aboutDialog);

    // 3. Создаем текстовый браузер (он поддерживает прокрутку по умолчанию)
    QTextBrowser *textBrowser = new QTextBrowser(aboutDialog);

    // 4. Загружаем текст из ресурсов
    QFile file("://README.md"); // Путь к вашему файлу в ресурсах
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Устанавливаем текст как Markdown (поддерживается в Qt 5.14+)
        textBrowser->setMarkdown(file.readAll());
        file.close();
    } else {
        textBrowser->setText("Не удалось загрузить файл справки README.md");
    }

    // Делаем так, чтобы ссылки открывались в обычном браузере
    textBrowser->setOpenExternalLinks(true);

    // 5. Добавляем виджет в лейаут и запускаем окно
    layout->addWidget(textBrowser);
    aboutDialog->setLayout(layout);

    // exec() делает окно модальным (пока не закроешь, в основное не вернешься)
    aboutDialog->exec();
}

MainWindow::~MainWindow()
{
    delete ui;
}
