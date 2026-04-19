#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QFile>
#include <QMessageBox>
#include <QSplitter>

#include "codeeditor.h"
#include "infowindow.h"
#include "lexer.h"
#include "parser.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QString m_lastAstTree;
    QString m_lastAstJson;
    void refreshAstOutput();
    CodeEditor* currentEditor();
    bool maybeSave(CodeEditor *editor);
    bool saveFile(CodeEditor *editor, const QString &path = "");
    void setupEditor(CodeEditor *editor, const QString &fileName);

private slots:
    void updateCursorPosition();
    // File menu
    void on_action_new_triggered();
    void on_action_open_triggered();
    void on_action_save_triggered();
    void on_action_save_as_triggered();
    void on_action_exit_triggered();
    void closeTab(int index);
    void updateTabTitle(bool modified);
    // Actions menu
    void on_action_copy_triggered();
    void on_action_cut_triggered();
    void on_action_paste_triggered();
    void on_action_undo_triggered();
    void on_action_redo_triggered();
    void on_action_delete_triggered();
    void on_action_select_all_triggered();
    // Info menu
    void on_action_about_triggered();
    void on_action_info_triggered();
    // Run program
    void on_action_run_triggered();
    void on_action_run_regex_triggered();
    void on_tableWidget_regex_itemSelectionChanged();
    void on_tableWidget_cellDoubleClicked(int row);
    void on_tableWidget_error_cellDoubleClicked(int row, int column);
    void on_tableWidget_error_cellClicked(int row, int column);
    void on_tableWidget_semantic_cellDoubleClicked(int row, int column);
    void on_tableWidget_semantic_cellClicked(int row, int column);

protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
