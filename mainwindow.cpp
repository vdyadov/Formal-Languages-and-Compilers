#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QSplitter *vSplitter = new QSplitter(Qt::Vertical, this);

    vSplitter->addWidget(ui->tabWidget);
    vSplitter->addWidget(ui->tableWidget);

    QGridLayout *mainLayout = qobject_cast<QGridLayout*>(ui->centralwidget->layout());
    if (mainLayout) {
        mainLayout->addWidget(vSplitter, 0, 0);
    }

    vSplitter->setStretchFactor(0, 60);
    vSplitter->setStretchFactor(1, 1);

    vSplitter->setHandleWidth(4);
    vSplitter->setStyleSheet("QSplitter::handle { background: #cccccc; }");

    // --- Файл ---
    ui->action_new->setShortcut(QKeySequence::New);       // Ctrl+N
    ui->action_open->setShortcut(QKeySequence::Open);     // Ctrl+O
    ui->action_save->setShortcut(QKeySequence::Save);     // Ctrl+S
    ui->action_exit->setShortcut(QKeySequence(Qt::Key_F10));

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
    ui->action_run->setShortcut(QKeySequence(Qt::Key_F5));

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, [this](int index) {
        QWidget* tab = ui->tabWidget->widget(index);
        ui->tabWidget->removeTab(index);
        delete tab;
    });

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateCursorPosition);

    // --- Настройка таблицы ---
    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setHorizontalHeaderLabels({"Условный код", "Тип лексемы", "Лексема", "Местоположение"});
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // Растянуть по ширине
}

void MainWindow::on_action_run_triggered() {
    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
    if (!editor) return;

    QString code = editor->toPlainText();

    Lexer scanner;
    QList<Token> tokens = scanner.tokenize(code);

    ui->tableWidget->setRowCount(0);

    for (const Token &t : std::as_const(tokens)) {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);

        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(t.code)));

        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(t.typeName));

        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(t.lexeme));

        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(t.getLocation()));

        if (t.code == -1) {
            for (int col = 0; col < 4; ++col) {
                ui->tableWidget->item(row, col)->setBackground(Qt::red);
                ui->tableWidget->item(row, col)->setForeground(Qt::white);
            }
        }
    }
}

void MainWindow::on_tableWidget_cellDoubleClicked(int row) {
    QString location = ui->tableWidget->item(row, 3)->text();

    int lineNum = location.section(' ', 1, 1).remove(',').toInt();
    int startPos = location.section(' ', 2, 2).section('-', 0, 0).toInt();

    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
    if (editor) {
        QTextBlock block = editor->document()->findBlockByLineNumber(lineNum - 1);
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, startPos - 1);
        editor->setTextCursor(cursor);
        editor->setFocus();
    }
}

void MainWindow::updateCursorPosition() {
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QTextCursor cursor = editor->textCursor();

    int line = cursor.blockNumber() + 1;
    int column = cursor.columnNumber() + 1;

    // TODO: переместить направо
    QString message = QString("Строка: %1 | Столбец: %2").arg(line).arg(column);
    ui->statusbar->showMessage(message);
}

CodeEditor* MainWindow::currentEditor() {
    return qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
}

void MainWindow::setupEditor(CodeEditor *editor, const QString &fullPath) {
    QString title = fullPath.isEmpty() ? "Untitled" : QFileInfo(fullPath).fileName();
    if (!fullPath.isEmpty()) editor->setProperty("filePath", fullPath);

    int index = ui->tabWidget->addTab(editor, title);
    ui->tabWidget->setCurrentIndex(index);

    connect(editor->document(), &QTextDocument::modificationChanged, this, &MainWindow::updateTabTitle);
}

void MainWindow::updateTabTitle(bool modified) {
    int idx = ui->tabWidget->currentIndex();
    QString title = ui->tabWidget->tabText(idx);
    if (modified && !title.endsWith("*")) ui->tabWidget->setTabText(idx, title + "*");
    else if (!modified && title.endsWith("*")) ui->tabWidget->setTabText(idx, title.left(title.length()-1));
}

void MainWindow::on_action_new_triggered() {
    CodeEditor *editor = new CodeEditor();

    connect(editor, &CodeEditor::cursorPositionChanged, this, &MainWindow::updateCursorPosition);

    ui->tabWidget->addTab(editor, "new.cpp");
    ui->tabWidget->setCurrentWidget(editor);
}

void MainWindow::on_action_open_triggered() {
    QString path = QFileDialog::getOpenFileName(this, "Открыть файл", "", "C++ Files (*.cpp *.h);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        CodeEditor *editor = new CodeEditor();
        connect(editor, &CodeEditor::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
        editor->setPlainText(file.readAll());
        file.close();
        setupEditor(editor, path);
        editor->document()->setModified(false);
    }
}

bool MainWindow::saveFile(CodeEditor *editor, const QString &path) {
    QString actualPath = path.isEmpty() ? editor->property("filePath").toString() : path;

    if (actualPath.isEmpty()) return false;

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
        if (ed->property("filePath").toString().isEmpty()) {
            on_action_save_as_triggered();
        } else {
            saveFile(ed);
            ui->statusbar->showMessage("Файл успешно сохранен", 3000);
        }
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
    close();
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

void MainWindow::on_action_about_triggered() {
    InfoWindow *aboutWin = new InfoWindow("О программе", "://README.md", this);

    aboutWin->setAttribute(Qt::WA_DeleteOnClose);

    aboutWin->show();
}

void MainWindow::on_action_info_triggered() {
    InfoWindow *helpWin = new InfoWindow("Справка", "://HELP.md", this);

    helpWin->setAttribute(Qt::WA_DeleteOnClose);

    helpWin->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}
