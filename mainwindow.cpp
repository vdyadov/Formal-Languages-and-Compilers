#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGridLayout>
#include <QJsonDocument>
#include <QFont>
#include <QFontDatabase>

#include "ast.h"
#include "semantic.h"

#include <memory>

#include <QRegularExpression>
#include <QTableWidget>
#include <QTextBlock>

namespace {

static void jumpToErrorLocation(QTableWidget *table, int row, int tabWidgetCol, QWidget *editorTabWidget)
{
    if (!table || row < 0 || !editorTabWidget)
        return;

    QTableWidgetItem *item = table->item(row, tabWidgetCol);
    if (!item)
        return;

    QString locText = item->text();

    static QRegularExpression re("(\\d+)");
    QRegularExpressionMatchIterator i = re.globalMatch(locText);

    QList<int> coords;
    while (i.hasNext())
        coords << i.next().captured(1).toInt();

    if (coords.size() < 2)
        return;

    const int targetLine = coords[0];
    const int targetCol = coords[1];

    CodeEditor *editor = qobject_cast<CodeEditor *>(editorTabWidget);
    if (!editor)
        return;

    QTextBlock block = editor->document()->findBlockByLineNumber(targetLine - 1);
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, targetCol - 1);
    editor->setTextCursor(cursor);
    editor->setFocus();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->horizontalLayout_search->setStretch(0, 0);
    ui->horizontalLayout_search->setStretch(1, 1);
    ui->horizontalLayout_search->setStretch(2, 0);

    QSplitter *vSplitter = new QSplitter(Qt::Vertical, this);

    vSplitter->addWidget(ui->tabWidget);
    vSplitter->addWidget(ui->searchBarWidget);
    vSplitter->addWidget(ui->tabWidget_error);

    connect(ui->regexPatternEdit, &QLineEdit::returnPressed, this, &MainWindow::on_action_run_regex_triggered);
    connect(ui->regexSearchButton, &QPushButton::clicked, this, &MainWindow::on_action_run_regex_triggered);

    QGridLayout *mainLayout = qobject_cast<QGridLayout*>(ui->centralwidget->layout());
    if (mainLayout) {
        mainLayout->addWidget(vSplitter, 0, 0);
    }

    vSplitter->setStretchFactor(0, 60);
    vSplitter->setStretchFactor(2, 1);

    vSplitter->setHandleWidth(4);
    vSplitter->setStyleSheet("QSplitter::handle { background: #cccccc; }");

    ui->tabWidget_error->setCurrentIndex(1);

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
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableWidget_error->setColumnCount(3);
    ui->tableWidget_error->setHorizontalHeaderLabels({"Неверный фрагмент", "Местоположение", "Описание"});
    ui->tableWidget_error->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableWidget_regex->setColumnCount(3);
    ui->tableWidget_regex->setHorizontalHeaderLabels({"Подстрока", "Позиция (Стр:Сим)", "Длина"});
    ui->tableWidget_regex->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableWidget_semantic->setColumnCount(3);
    ui->tableWidget_semantic->setHorizontalHeaderLabels({"Фрагмент", "Местоположение", "Описание"});
    ui->tableWidget_semantic->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->comboBox_astFormat->clear();
    ui->comboBox_astFormat->addItem(QStringLiteral("Дерево"));
    ui->comboBox_astFormat->addItem(QStringLiteral("JSON"));

    QFont mono(QStringLiteral("Consolas"));
    if (!mono.exactMatch())
        mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    ui->plainTextEdit_ast->setFont(mono);

    if (ui->gridLayout_ast)
        ui->gridLayout_ast->setRowStretch(1, 1);

    connect(ui->comboBox_astFormat, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshAstOutput);
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
        if (t.code == -1) ui->tableWidget->item(row, 2)->setBackground(Qt::red);
    }

    Parser parser(tokens);
    QList<SyntaxError> errors = parser.parse();
    std::unique_ptr<ProgramNode> rawAst = parser.takeProgram();

    SemanticAnalyzer::Result semantic = SemanticAnalyzer::analyze(std::move(rawAst));

    ui->tableWidget_error->setRowCount(0);

    ui->tabWidget_error->setCurrentIndex(1);

    if (errors.isEmpty()) {
        ui->tableWidget_error->insertRow(0);
        ui->tableWidget_error->setItem(0, 2, new QTableWidgetItem("Синтаксических ошибок нет!"));
    } else {
        for (const auto &err : std::as_const(errors)) {
            int row = ui->tableWidget_error->rowCount();
            ui->tableWidget_error->insertRow(row);
            ui->tableWidget_error->setItem(row, 0, new QTableWidgetItem(err.fragment));
            ui->tableWidget_error->setItem(row, 1, new QTableWidgetItem(QString("Стр %1, Поз %2").arg(err.line).arg(err.col)));
            ui->tableWidget_error->setItem(row, 2, new QTableWidgetItem(err.description));
        }
    }

    ui->tableWidget_semantic->setRowCount(0);
    if (semantic.errors.isEmpty()) {
        ui->tableWidget_semantic->insertRow(0);
        ui->tableWidget_semantic->setItem(0, 2, new QTableWidgetItem(QStringLiteral("Семантических ошибок нет!")));
    } else {
        for (const auto &err : std::as_const(semantic.errors)) {
            int row = ui->tableWidget_semantic->rowCount();
            ui->tableWidget_semantic->insertRow(row);
            ui->tableWidget_semantic->setItem(row, 0, new QTableWidgetItem(err.fragment));
            ui->tableWidget_semantic->setItem(row, 1, new QTableWidgetItem(QStringLiteral("Стр %1, Поз %2").arg(err.line).arg(err.col)));
            ui->tableWidget_semantic->setItem(row, 2, new QTableWidgetItem(err.description));
        }
    }

    if (semantic.program) {
        m_lastAstTree = formatTree(*semantic.program);
        m_lastAstJson = QString::fromUtf8(QJsonDocument(toJson(*semantic.program)).toJson(QJsonDocument::Indented));
    } else {
        m_lastAstTree.clear();
        m_lastAstJson.clear();
    }
    refreshAstOutput();

    statusBar()->showMessage(QStringLiteral("Синтаксических ошибок: %1. Семантических ошибок: %2.")
                                 .arg(errors.size())
                                 .arg(semantic.errors.size()));

    // Добавляем финальную строку в таблицу
    // int lastRow = ui->tableWidget_error->rowCount();
    // ui->tableWidget_error->insertRow(lastRow);

    // auto *totalLabel = new QTableWidgetItem("Общее количество ошибок:");
    // totalLabel->setTextAlignment(Qt::AlignCenter);
    // totalLabel->setBackground(QColor(240, 240, 240));

    // ui->tableWidget_error->setItem(lastRow, 0, totalLabel);
    // ui->tableWidget_error->setSpan(lastRow, 0, 1, 2); // Объединяем 2 колонки

    // auto *countItem = new QTableWidgetItem(QString::number(errors.size()));
    // countItem->setTextAlignment(Qt::AlignCenter);
    // ui->tableWidget_error->setItem(lastRow, 2, countItem);
}

void MainWindow::on_tableWidget_error_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    jumpToErrorLocation(ui->tableWidget_error, row, 1, ui->tabWidget->currentWidget());
}

void MainWindow::on_tableWidget_error_cellClicked(int row, int column)
{
    Q_UNUSED(column);
    on_tableWidget_error_cellDoubleClicked(row, column);
}

void MainWindow::refreshAstOutput()
{
    if (ui->comboBox_astFormat->currentIndex() == 1)
        ui->plainTextEdit_ast->setPlainText(m_lastAstJson);
    else
        ui->plainTextEdit_ast->setPlainText(m_lastAstTree);
}

void MainWindow::on_tableWidget_semantic_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    jumpToErrorLocation(ui->tableWidget_semantic, row, 1, ui->tabWidget->currentWidget());
}

void MainWindow::on_tableWidget_semantic_cellClicked(int row, int column)
{
    Q_UNUSED(column);
    on_tableWidget_semantic_cellDoubleClicked(row, column);
}

QList<SearchResult> findIdentifierCustom(const QString &text) {
    QList<SearchResult> results;

    QStringList lines = text.split('\n');
    int currentAbsPos = 0;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) {
            currentAbsPos += lines[i].length() + 1;
            continue;
        }

        int state = 0;
        int count = 0;
        bool ok = true;

        for (QChar ch : line) {
            if (state == 0) {
                if (ch.isLetter()) {
                    state = 1;
                    count++;
                } else {
                    ok = false; break;
                }
            } else if (state == 1 || state == 2) {
                if (ch.isLetterOrNumber() || ch == '_') {
                    state = 2;
                    count++;
                } else {
                    ok = false; break;
                }
            }
        }

        if (ok && count >= 2 && count <= 30) {
            int lineStartPos = text.indexOf(line, currentAbsPos);
            results.append({line, i + 1, 1, count, lineStartPos});
        }

        currentAbsPos += lines[i].length() + 1;
    }
    return results;
}

void MainWindow::on_action_run_regex_triggered() {
    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
    if (!editor) return;

    QString text = editor->toPlainText();
    if (text.isEmpty()) {
        editor->clearRegexMatchHighlights();
        QMessageBox::information(this, "Поиск", "Нет данных для поиска");
        return;
    }

    QString pattern = ui->regexPatternEdit->text().trimmed();
    if (pattern.isEmpty()) {
        switch (ui->searchTypeCombo->currentIndex()) {
        case 0: pattern = "^\\d{9}$"; break;
        case 1: pattern = "^[a-zA-Zа-яёА-ЯЁ][a-zA-Zа-яёА-ЯЁ0-9]{1,29}$"; break;
        case 2: pattern = "^([A-Z2-7]{8})*([A-Z2-7]{2}={6}|[A-Z2-7]{4}={4}|[A-Z2-7]{5}={3}|[A-Z2-7]{7}=)?$"; break;
        }
    }

    QRegularExpression rex(pattern);
    if (!rex.isValid()) {
        editor->clearRegexMatchHighlights();
        QMessageBox::warning(this, "Поиск регулярных выражений",
                             QString("Некорректное регулярное выражение:\n%1").arg(rex.errorString()));
        return;
    }
    rex.setPatternOptions(QRegularExpression::MultilineOption);

    QRegularExpressionMatchIterator it = rex.globalMatch(text);

    ui->tableWidget_regex->setRowCount(0);
    int count = 0;
    QList<QPair<int, int>> highlightRanges;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        
        int groupIndex = (match.lastCapturedIndex() >= 1) ? 1 : 0;
        
        QString resultText = match.captured(groupIndex);
        int pos = match.capturedStart(groupIndex);
        int len = match.capturedLength(groupIndex);
    
        if (resultText.isEmpty() && len == 0) continue;
    
        int row = ui->tableWidget_regex->rowCount();
        ui->tableWidget_regex->insertRow(row);
    
        highlightRanges.append({pos, len});
    
        int lineNum = text.left(pos).count('\n') + 1;
        int lastNewLine = text.left(pos).lastIndexOf('\n');
        int colNum = (lastNewLine == -1) ? pos + 1 : pos - lastNewLine;
    
        ui->tableWidget_regex->setItem(row, 0, new QTableWidgetItem(resultText)); 
        ui->tableWidget_regex->setItem(row, 1, new QTableWidgetItem(QString("%1:%2").arg(lineNum).arg(colNum)));
        ui->tableWidget_regex->setItem(row, 2, new QTableWidgetItem(QString::number(len)));
    
        ui->tableWidget_regex->item(row, 0)->setData(Qt::UserRole, pos);
        count++;
    }

    editor->setRegexMatchHighlights(highlightRanges);

    ui->statusbar->showMessage(QString("Найдено совпадений: %1").arg(count));
}

void MainWindow::on_tableWidget_regex_itemSelectionChanged() {
    int row = ui->tableWidget_regex->currentRow();
    if (row < 0) return;

    int pos = ui->tableWidget_regex->item(row, 0)->data(Qt::UserRole).toInt();
    int length = ui->tableWidget_regex->item(row, 2)->text().toInt();

    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
    if (editor) {
        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(pos);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, length);

        editor->setTextCursor(cursor);
        editor->setFocus();
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
