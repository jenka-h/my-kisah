#pragma once

#include "application/DecompilerSession.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace mykisah::gui {

class CallGraphView;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void open_binary(const QString& path);

private:
    void build_ui();
    void choose_binary();
    void populate_functions();
    void filter_functions(const QString& query);
    void select_function_item();
    void show_function(const app::FunctionResult& function);
    void navigate_to_address(uint64_t address);
    void activate_assembly_item(QTreeWidgetItem* item);
    void patch_selected_instruction();

    app::DecompilerSession session_;

    QAction* open_action_ = nullptr;
    QAction* patch_action_ = nullptr;
    QLineEdit* search_ = nullptr;
    QListWidget* functions_ = nullptr;
    QLabel* address_ = nullptr;
    QPlainTextEdit* decompiled_ = nullptr;
    QTreeWidget* assembly_ = nullptr;
    CallGraphView* call_graph_ = nullptr;
};

} // namespace mykisah::gui
