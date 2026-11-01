#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_TZ_Project.h"

class TZ_Project : public QMainWindow
{
    Q_OBJECT

public:
    TZ_Project(QWidget *parent = nullptr);
    ~TZ_Project();

private:
    Ui::TZ_ProjectClass ui;
};

