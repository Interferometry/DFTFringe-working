/******************************************************************************
**
**  Copyright 2016 Dale Eason
**  This file is part of DFTFringe
**  is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation version 3 of the License

** DFTFringe is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with DFTFringe.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************************/
#include "surfaceanalysistools.h"
#include "ui_surfaceanalysistools.h"
#include <QDebug>
#include <QSettings>
#include <QtAlgorithms>
#include <QLineEdit>
#include "mirrordlg.h"
#include "renamewavefrontdlg.h"
#include <QMessageBox>
surfaceAnalysisTools *surfaceAnalysisTools::m_Instance = NULL;

surfaceAnalysisTools * surfaceAnalysisTools::get_Instance(QWidget *parent){
    if  (m_Instance == NULL){
        m_Instance = new surfaceAnalysisTools(parent);
    }
    return m_Instance;
}

surfaceAnalysisTools::surfaceAnalysisTools(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::surfaceAnalysisTools),lastCurrentItem(-1)
{
    ui->setupUi(this);
    //ui->deleteWave->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_edgeMaskLabel = ui->outsideEdgeMaskLabel;
    m_centerMaskLabel = ui->insideEdgeMaskLabel;
    m_blurrRadius = ui->blurMm;
    QSettings settings;
    bool ch = settings.value("GBlur", true).toBool();
    double val = settings.value("GBValue", 20).toInt();
    ui->surfaceSmoothGausianBlurr->setValue(val);
    ui->blurCB->setCheckState((ch) ? Qt::Checked : Qt::Unchecked);
    m_useDefocus = false;
    m_defocus = 0.;
    connect(&m_defocusTimer, SIGNAL(timeout()), this, SLOT(defocusTimerDone()));
    connect(ui->wavefrontList->itemDelegate(), SIGNAL(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)), this,
            SLOT(ListWidgetEditEnd(QWidget*, QAbstractItemDelegate::EndEditHint)));
    ui->wavefrontList->setContextMenuPolicy(Qt::CustomContextMenu);
}

void surfaceAnalysisTools::enableControls(bool flag){

    setEnabled(flag);
}


void surfaceAnalysisTools::setBlurText(QString txt){
    ui->blurMm->setText(txt);
}
void ListWidgetEditEnd(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);
void surfaceAnalysisTools::addWaveFront(const QString &name){

    QStringList list = name.split('/');
    QString shorter = name;
    if (list.size() > 1)
        shorter = list[list.size()-2] + "/" + list[list.size()-1];//fontMetrics().elidedText(name,Qt::ElideLeft,w);

    ui->wavefrontList->addItem(shorter);
    lastCurrentItem = ui->wavefrontList->count()-1;

}

void surfaceAnalysisTools::removeWaveFront(const QString &){

}

surfaceAnalysisTools::~surfaceAnalysisTools()
{
    delete ui;
}

void surfaceAnalysisTools::on_blurCB_clicked(bool checked)
{
    ui->surfaceSmoothGausianBlurr->setEnabled(checked);
    emit surfaceSmoothGBEnabled(checked);
}

void surfaceAnalysisTools::on_wavefrontList_itemDoubleClicked(QListWidgetItem *item)
{

    ui->wavefrontList->editItem(item);
    emit wavefrontDClicked(item->text());

}

void surfaceAnalysisTools::on_spinBox_valueChanged(int arg1)
{
    emit outsideMaskValue(arg1);
}

void surfaceAnalysisTools::on_spinBox_2_valueChanged(int arg1)
{
    emit centerMaskValue(arg1);
}

 void surfaceAnalysisTools::currentNdxChanged(int ndx){
     if (ui->wavefrontList->count() == 0)
         return;

     QListWidgetItem *item = ui->wavefrontList->currentItem();
     if (item)
         item->setTextColor(Qt::gray);
     ui->wavefrontList->setCurrentRow(ndx,QItemSelectionModel::Current);
     item = ui->wavefrontList->currentItem();
     item->setTextColor(Qt::black);
     lastCurrentItem = ui->wavefrontList->currentRow();


 }
void surfaceAnalysisTools::deleteWaveFront(int i){
    QListWidgetItem* item = ui->wavefrontList->takeItem(i);
    delete item;

}

void surfaceAnalysisTools::on_wavefrontList_clicked(const QModelIndex &index)
{

    QListWidgetItem *item = ui->wavefrontList->item(lastCurrentItem);
    if (item)
        item->setTextColor(Qt::gray);

    QModelIndexList indexes = ui->wavefrontList->selectionModel()->selectedIndexes();

    /*
    foreach(QModelIndex index, indexes)
    {
        emit waveFrontClicked(index.row());
    }
            */
    // only enform about the last item in the list for efficency.
    //That way surface manager does not spend time updating things not shown.
    emit waveFrontClicked(indexes.last().row());
    currentNdxChanged(index.row());
}
QList<int> surfaceAnalysisTools::SelectedWaveFronts(){
    QModelIndexList indexes = ui->wavefrontList->selectionModel()->selectedIndexes();

    QList<int> indexList;
    foreach(QModelIndex index, indexes)
    {
        indexList << index.row();
    }
    return indexList;
}

void surfaceAnalysisTools::on_deleteWave_clicked()
{
    QModelIndexList indexes = ui->wavefrontList->selectionModel()->selectedIndexes();

    QList<int> indexList = SelectedWaveFronts();
    qSort(indexList.begin(), indexList.end(), qGreater<int>());
    emit deleteTheseWaveFronts(indexList);
}




void surfaceAnalysisTools::on_transformsPB_clicked()
{
    emit doxform(SelectedWaveFronts());
}

void surfaceAnalysisTools::on_averagePB_clicked()
{
    emit average(SelectedWaveFronts());
}
void surfaceAnalysisTools::select(int item){
    ui->wavefrontList->selectionModel()->reset();
    if (ui->wavefrontList->count() > 0)
        ui->wavefrontList->item(item)->setSelected(true);
}

void surfaceAnalysisTools::on_SelectAllPB_clicked()
{
    ui->wavefrontList->selectAll();
}

void surfaceAnalysisTools::on_SelectNonePB_clicked()
{
    ui->wavefrontList->selectionModel()->reset();
}
void surfaceAnalysisTools::nameChanged(int ndx, QString newname){
    ui->wavefrontList->item(ndx)->setText(newname);
}

void surfaceAnalysisTools::nameChanged(QString old, QString newname){

    // first look for full length name match
    QList<QListWidgetItem *> ql = ui->wavefrontList->findItems(old,Qt::MatchExactly);
    if (ql.size() == 1){
        ql[0]->setText(newname);
        return;
    }

    QStringList list = old.split('/');
    QString shorter = old;

    if (list.size() > 1)
        shorter = list[list.size()-2] + "/" + list[list.size()-1];

    ql = ui->wavefrontList->findItems(shorter,Qt::MatchEndsWith);
    if (ql.size()) {

        list = newname.split('/');
        QString shorter = newname;

        if (list.size() > 1)
            shorter = list[list.size()-2] + "/" + list[list.size()-1];
        ql[0]->setText(shorter);
    }
}



void surfaceAnalysisTools::defocusTimerDone(){
    m_defocusTimer.stop();
    emit defocusChanged();
}

#include "defocusdlg.h"
void surfaceAnalysisTools::defocusControlChanged(double val){
    m_defocus = val;
    emit defocusChanged();

}


void surfaceAnalysisTools::on_InvertPB_pressed()
{
    emit invert(SelectedWaveFronts());
}

void surfaceAnalysisTools::on_wavefrontList_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex t = ui->wavefrontList->indexAt(pos);
    QListWidgetItem *item = ui->wavefrontList->item(t.row());
    if (item == 0)
        return;
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->wavefrontList->item(t.row())->setSelected(true);	// even a right click will select the item
    ui->wavefrontList->editItem(item);
}

void surfaceAnalysisTools::ListWidgetEditEnd(QWidget *editor, QAbstractItemDelegate::EndEditHint ){
    QString NewValue = reinterpret_cast<QLineEdit*>(editor)->text();
    QModelIndexList indexes = ui->wavefrontList->selectionModel()->selectedIndexes();
    if (indexes.length() == 1){
        emit wftNameChanged(indexes[0].row(), NewValue);
    }
    //emit wftNameChanged(t.row(), item->text());
}

void surfaceAnalysisTools::on_pushButton_clicked()
{
    emit updateSelected();
}


void surfaceAnalysisTools::on_filterPB_clicked()
{
    emit filterWavefronts();
}



void surfaceAnalysisTools::on_surfaceSmoothGausianBlurr_valueChanged(double arg1)
{
   emit     surfaceSmoothGBValue(arg1);
}






void surfaceAnalysisTools::on_defocus_clicked()
{
    if (ui->wavefrontList->count() == 0){
        return;
    }
    m_useDefocus = true;
    emit defocusSetup();

    defocusDlg *dlg = new defocusDlg();
    connect(dlg, SIGNAL(defocus(double)), this, SLOT(defocusControlChanged(double)));
    connect(dlg, SIGNAL(finished(int)), this, SLOT(closeDefocus(int)) );
    dlg->show();

return;
}

void surfaceAnalysisTools::closeDefocus(int /*result*/){
    m_useDefocus = false;
    m_defocus = 0.;
    emit defocusChanged();
}
