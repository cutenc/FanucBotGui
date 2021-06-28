#include "cdrillbottaskdialog.h"
#include "ui_cdrillbottaskdialog.h"

#include "../gui_types.h"

CDrillBotTaskDialog::CDrillBotTaskDialog(QWidget *parent, const GUI_TYPES::STaskPoint &initData) :
    QDialog(parent),
    ui(new Ui::CDrillBotTaskDialog)
{
    ui->setupUi(this);

    ui->dsbGlobalX->setValue(initData.globalPos.x);
    ui->dsbGlobalY->setValue(initData.globalPos.y);
    ui->dsbGlobalZ->setValue(initData.globalPos.z);

    connect(ui->pbOk, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(ui->pbCancel, &QAbstractButton::clicked, this, &QDialog::reject);
}

CDrillBotTaskDialog::~CDrillBotTaskDialog()
{
    delete ui;
}

GUI_TYPES::STaskPoint CDrillBotTaskDialog::getTaskPoint() const
{
    GUI_TYPES::STaskPoint res;
    res.taskType = GUI_TYPES::ENBTT_DRILL;
    res.globalPos.x = ui->dsbGlobalX->value();
    res.globalPos.y = ui->dsbGlobalY->value();
    res.globalPos.z = ui->dsbGlobalZ->value();
    res.angle.x = ui->dsbApha->value();
    res.angle.y = ui->dsbBeta->value();
    res.angle.z = ui->dsbGamma->value();
    return res;
}
