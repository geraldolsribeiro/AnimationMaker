/****************************************************************************
** Copyright (C) 2016 Olaf Japp
**
** This file is part of AnimationMaker.
**
**  AnimationMaker is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  AnimationMaker is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with AnimationMaker.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "mainwindow.h"
#include "animationscene.h"
#include "vectorgraphic.h"
#include "bitmap.h"
#include "exception.h"
#include "news.h"
#include "itempropertyeditor.h"
#include "timeline.h"
#include "scenepropertyeditor.h"
#include "transitioneditor.h"
#include "expander.h"
#include "rectangle.h"
#include "ellipse.h"
#include "text.h"
#include "bitmap.h"
#include "vectorgraphic.h"
#include "keyframe.h"
#include <QtTest/QTest>
#include <QMessageBox>
#include <QGraphicsSvgItem>
#include <QTreeWidget>
#include <QMainWindow>
#include <QtWidgets>
#include <QTextBrowser>

#define MAGIC 0x414D4200
#define FILE_VERSION 100

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    undoStack = new QUndoStack(this);

    setDockNestingEnabled(true);
    createStatusBar();
    createActions();
    createMenus();
    createGui();
    readSettings();
}

MainWindow::~MainWindow()
{
    delete scene;
    delete timeline;
    delete elementTree;
    delete view;
}

void MainWindow::save()
{
    writeFile(loadedFile.filePath());
}

void MainWindow::saveAs()
{
    QString fileName;
    QFileDialog *dialog = new QFileDialog();
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(tr("AnimationMaker (*.amb);;All Files (*)"));
    dialog->setWindowTitle(tr("Save Animation"));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    if(dialog->exec())
        fileName = dialog->selectedFiles().first();
    delete dialog;
    if(fileName.isEmpty())
        return;

    writeFile(fileName);
    loadedFile.setFile(fileName);
    saveAct->setEnabled(true);
    setTitle();
}

void MainWindow::writeFile(QString fileName)
{
    scene->clearSelection();
    timeline->setPlayheadPosition(0);

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "Error", "Unable to open file " + fileName);
        return;
    }
    QDataStream out(&file);

    // Write a header with a "magic number" and a version
    out << (quint32)MAGIC;
    out << (qint32)FILE_VERSION;
    out.setVersion(QDataStream::Qt_5_7);

    out << scene;

    file.close();

    undoStack->clear();
}

void MainWindow::setTitle()
{
    if(loadedFile.completeBaseName().isEmpty())
        setWindowTitle(QCoreApplication::applicationName());
    else
        setWindowTitle(QCoreApplication::applicationName() + " - " + loadedFile.completeBaseName() + "." + loadedFile.suffix());
}

void MainWindow::reset()
{
    setCentralWidget(splitter);
    scene->reset();
    timeline->reset();
}

void MainWindow::newfile()
{
    reset();
    fillTree();
    saveAct->setEnabled(false);
    loadedFile.setFile("");
    setTitle();
    m_scenePropertyEditor->setScene(scene);
    propertiesdock->setWidget(m_scenePropertyEditor);
}

void MainWindow::open()
{
    QString fileName;
    QFileDialog *dialog = new QFileDialog();
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(tr("AnimationMaker (*.amb);;All Files (*)"));
    dialog->setWindowTitle(tr("Open Animation"));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    if(dialog->exec())
        fileName = dialog->selectedFiles().first();
    delete dialog;
    if (fileName.isEmpty())
        return;
    reset();
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Error", "Unable to open file " + fileName);
        return;
    }
    QDataStream in(&file);

    // Read and check the header
    quint32 magic;
    in >> magic;
    if (magic != MAGIC)
    {
        file.close();
        QMessageBox::warning(this, "AnimationMaker", "This file is not a valid AnimationMaker file!");
        return;
    }

    // Read the version
    qint32 version;
    in >> version;
    if (version < FILE_VERSION)
    {
        file.close();
        QMessageBox::warning(this, "AnimationMaker", "This file is not a valid AnimationMaker file!");
        return;
    }
    if (version > FILE_VERSION)
    {
        file.close();
        QMessageBox::warning(this, "AnimationMaker", "This file is not a valid AnimationMaker file!");
        return;
    }

    //    if (version <= 110)
    //        in.setVersion(QDataStream::Qt_4_0);
    //    else
    //        in.setVersion(QDataStream::Qt_5_7);

    // Read the data
    in >> scene;
    file.close();

    fillTree();
    elementTree->expandAll();
    m_scenePropertyEditor->setScene(scene);
    timeline->expandTree();
    loadedFile.setFile(fileName);
    saveAct->setEnabled(true);
    setTitle();
}

void MainWindow::fillTree()
{
    for(int i=root->childCount() - 1; i >= 0; i--)
    {
        QTreeWidgetItem *treeItem= root->child(i);
        root->removeChild(treeItem);
        delete treeItem;
    }

    QList<QGraphicsItem*> itemList = scene->items(Qt::AscendingOrder);
    foreach (QGraphicsItem *item, itemList)
    {
        ResizeableItem *ri = dynamic_cast<ResizeableItem*>(item);
        if(ri)
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem();
            treeItem->setText(0, ri->id());
            treeItem->setIcon(0, QIcon(":/images/rect.png"));
            treeItem->setData(0, 3, qVariantFromValue((void *) ri));
            root->addChild(treeItem);
            connect(ri, SIGNAL(idChanged(ResizeableItem *, QString)), this, SLOT(idChanged(ResizeableItem *, QString)));
        }
    }
}

void MainWindow::idChanged(ResizeableItem *item, QString id)
{
    for(int i=0; i < root->childCount(); i++)
    {
        if(root->child(i)->data(0, 3).value<void *>() == item)
        {
            root->child(i)->setText(0, id);
            break;
        }
    }
}

void MainWindow::createGui()
{
    QToolBar *toolpanel = new QToolBar();
    toolpanel->setOrientation(Qt::Vertical);
    QActionGroup *anActionGroup = new QActionGroup(toolpanel);
    selectAct = new QAction("Select", anActionGroup);
    selectAct->setIcon(QIcon(":/images/arrow.png"));
    selectAct->setCheckable(true);

    rectangleAct = new QAction("Rectangle", anActionGroup);
    rectangleAct->setIcon(QIcon(":/images/rectangle.png"));
    rectangleAct->setCheckable(true);

    ellipseAct = new QAction("Ellipse", anActionGroup);
    ellipseAct->setIcon(QIcon(":/images/ellipse.png"));
    ellipseAct->setCheckable(true);

    textAct = new QAction("Text", anActionGroup);
    textAct->setIcon(QIcon(":/images/text.png"));
    textAct->setCheckable(true);

    bitmapAct = new QAction("Bitmap", anActionGroup);
    bitmapAct->setIcon(QIcon(":/images/camera.png"));
    bitmapAct->setCheckable(true);

    svgAct = new QAction("Vectorgraphic", anActionGroup);
    svgAct->setIcon(QIcon(":/images/svg.png"));
    svgAct->setCheckable(true);

    connect(selectAct, SIGNAL(triggered()), this, SLOT(setSelectMode()));
    connect(rectangleAct, SIGNAL(triggered()), this, SLOT(setRectangleMode()));
    connect(ellipseAct, SIGNAL(triggered()), this, SLOT(setEllipseMode()));
    connect(textAct, SIGNAL(triggered()), this, SLOT(setTextMode()));
    connect(bitmapAct, SIGNAL(triggered()), this, SLOT(setBitmapMode()));
    connect(svgAct, SIGNAL(triggered()), this, SLOT(setSvgMode()));

    toolpanel->addAction(selectAct);
    toolpanel->addAction(rectangleAct);
    toolpanel->addAction(ellipseAct);
    toolpanel->addAction(textAct);
    toolpanel->addAction(bitmapAct);
    toolpanel->addAction(svgAct);

    selectAct->toggle();

    News *news = new News("https://artanidos.github.io/AnimationMaker/news.html");
    newsdock = new QDockWidget(tr("News"), this);
    newsdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    newsdock->setWidget(news);
    newsdock->setObjectName("News");
    addDockWidget(Qt::RightDockWidgetArea, newsdock);

    tooldock = new QDockWidget(tr("Tools"), this);
    tooldock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    tooldock->setWidget(toolpanel);
    tooldock->setObjectName("Tools");
    addDockWidget(Qt::LeftDockWidgetArea, tooldock);

    scene = new AnimationScene();
    scene->registerUndoStack(undoStack);

    m_itemPropertyEditor = new ItemPropertyEditor();
    m_scenePropertyEditor = new ScenePropertyEditor();
    m_transitionEditor = new TransitionEditor();
    m_transitionEditor->setUndoStack(undoStack);

    m_scenePropertyEditor->setScene(scene);

    propertiesdock = new QDockWidget(tr("Properties"), this);
    propertiesdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    propertiesdock->setWidget(m_scenePropertyEditor);
    propertiesdock->setObjectName("Properties");

    addDockWidget(Qt::RightDockWidgetArea, propertiesdock);

    view = new QGraphicsView(scene);
    view->setSceneRect(-100, -100, scene->width() + 200, scene->height() + 200);
    view->setRenderHint(QPainter::RenderHint::Antialiasing);
    connect(scene, SIGNAL(selectionChanged()), this, SLOT(sceneSelectionChanged()));
    connect(scene, SIGNAL(itemAdded(QGraphicsItem*)), this, SLOT(sceneItemAdded(QGraphicsItem*)));
    connect(scene, SIGNAL(sizeChanged(int,int)), this, SLOT(sceneSizeChanged(int, int)));
    connect(scene, SIGNAL(itemRemoved(ResizeableItem*)), this, SLOT(sceneItemRemoved(ResizeableItem*)));
    connect(scene, SIGNAL(animationResetted()), this, SLOT(reset()));

    elementTree = new QTreeWidget();
    elementTree->header()->close();
    elementTree->setSelectionMode(QAbstractItemView::SingleSelection);
    root = new QTreeWidgetItem();
    root->setText(0, "Scene");
    elementTree->addTopLevelItem(root);
    connect(elementTree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(elementTreeItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

    elementsdock = new QDockWidget(tr("Elements"), this);
    elementsdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    elementsdock->setWidget(elementTree);
    elementsdock->setObjectName("Elements");
    addDockWidget(Qt::LeftDockWidgetArea, elementsdock);
    splitDockWidget(tooldock, elementsdock, Qt::Horizontal);

    timeline = new Timeline(scene);
    timeline->setMinimumHeight(110);

    connect(timeline, SIGNAL(itemSelectionChanged(ResizeableItem *)), this, SLOT(timelineSelectionChanged(ResizeableItem*)));
    connect(timeline, SIGNAL(transitionSelectionChanged(KeyFrame*)), this, SLOT(transitionSelectionChanged(KeyFrame*)));
    connect(m_itemPropertyEditor, SIGNAL(addKeyFrame(ResizeableItem*,QString,QVariant)), timeline, SLOT(addKeyFrame(ResizeableItem*,QString,QVariant)));

    splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(view);
    splitter->addWidget(timeline);

    setCentralWidget(splitter);
}

void MainWindow::elementTreeItemChanged(QTreeWidgetItem *newItem, QTreeWidgetItem *)
{
    scene->clearSelection();
    ResizeableItem *item = (ResizeableItem *)  newItem->data(0, 3).value<void *>();
    if(item)
    {
        item->setSelected(true);
        m_itemPropertyEditor->setItem(item);
        propertiesdock->setWidget(m_itemPropertyEditor);
    }
    else
    {
        propertiesdock->setWidget(m_scenePropertyEditor);
    }
}

void MainWindow::sceneSizeChanged(int width, int height)
{
    view->setSceneRect(-100, -100, width + 200, height + 200);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}

void MainWindow::writeSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
}

void MainWindow::readSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,  QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty())
    {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2, (availableGeometry.height() - height()) / 2);
    }
    else
    {
        restoreGeometry(geometry);
        restoreState(settings.value("state").toByteArray());
    }
}

void MainWindow::createActions()
{
    openAct = new QAction(tr("&Open..."), this);
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    newAct = new QAction(tr("&New"), this);
    connect(newAct, SIGNAL(triggered()), this, SLOT(newfile()));

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setEnabled(false);
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

    saveAsAct = new QAction(tr("Save &As..."), this);
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

    exportXmlAct = new QAction(tr("Export XML"), this);
    connect(exportXmlAct, SIGNAL(triggered()), this, SLOT(exportXml()));

    exportMovieAct = new QAction(tr("Export Movie"), this);
    connect(exportMovieAct, SIGNAL(triggered()), this, SLOT(exportMovie()));

    importXmlAct = new QAction(tr("Import XML"), this);
    connect(importXmlAct, SIGNAL(triggered()), this, SLOT(importXml()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    undoAct = undoStack->createUndoAction(this, tr("&Undo"));
    undoAct->setShortcuts(QKeySequence::Undo);

    redoAct = undoStack->createRedoAction(this, tr("&Redo"));
    redoAct->setShortcuts(QKeySequence::Redo);

    copyAct = new QAction(tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));

    pasteAct = new QAction(tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));

    delAct = new QAction(tr("&Delete"), this);
    delAct->setShortcut(QKeySequence::Delete);
    connect(delAct, SIGNAL(triggered()), this, SLOT(del()));

    showElementsAct = new QAction("Elements");
    connect(showElementsAct, SIGNAL(triggered()), this, SLOT(showElementsPanel()));

    showPropertyPanelAct = new QAction("Properties");
    connect(showPropertyPanelAct, SIGNAL(triggered()), this, SLOT(showPropertyPanel()));

    showToolPanelAct = new QAction("Tools");
    connect(showToolPanelAct, SIGNAL(triggered()), this, SLOT(showToolPanel()));

    showNewsPanelAct = new QAction("News");
    connect(showNewsPanelAct, SIGNAL(triggered()), this, SLOT(showNewsPanel()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    addAction(copyAct);
    addAction(pasteAct);
    addAction(exitAct);
    addAction(delAct);
}

void MainWindow::createMenus()
{
    importMenu = new QMenu(tr("Import"));
    importMenu->setEnabled(false);

    exportMenu = new QMenu(tr("Export"));
    exportMenu->setEnabled(false);

    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(importXmlAct);
    fileMenu->addAction(exportXmlAct);
    fileMenu->addAction(exportMovieAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);
    editMenu->addAction(delAct);

    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(showToolPanelAct);
    viewMenu->addAction(showElementsAct);
    viewMenu->addAction(showPropertyPanelAct);
    viewMenu->addAction(showNewsPanelAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);

    this->addAction(undoAct);
    this->addAction(redoAct);
    this->addAction(copyAct);
    this->addAction(pasteAct);
}

void MainWindow::about()
{
    QMessageBox msg;
    msg.setWindowTitle("About AnimationMaker (Community Edition)");
    msg.setText("AnimationMaker\nVersion: " + QCoreApplication::applicationVersion() + "\n(C) Copyright 2017 Olaf Japp. All rights reserved.\n\nThe program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.");
    msg.setIconPixmap(QPixmap(":/images/logo.png"));
    msg.exec();
}

void MainWindow::setSelectMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeSelect);
}

void MainWindow::setRectangleMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeRectangle);
}

void MainWindow::setEllipseMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeEllipse);
}

void MainWindow::setTextMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeText);
}

void MainWindow::setBitmapMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeBitmap);
}

void MainWindow::setSvgMode()
{
    scene->setEditMode(AnimationScene::EditMode::ModeSvg);
}

void MainWindow::sceneSelectionChanged()
{
    ResizeableItem *item = NULL;

    if(scene->selectedItems().count())
        item = dynamic_cast<ResizeableItem*>(scene->selectedItems().first());
    if(item)
    {
        m_itemPropertyEditor->setItem(item);
        propertiesdock->setWidget(m_itemPropertyEditor);
        for(int i=0; i<root->childCount(); i++)
        {
            QTreeWidgetItem *treeItem = root->child(i);
            if(treeItem->data(0, 3).value<void *>() == item)
                treeItem->setSelected(true);
            else
                treeItem->setSelected(false);
        }
        root->setSelected(false);

        timeline->selectItem(item);
    }
    else
    {
        propertiesdock->setWidget(m_scenePropertyEditor);
        for(int i=0; i<root->childCount(); i++)
            root->child(i)->setSelected(false);
        root->setSelected(true);
        timeline->selectItem(NULL);
    }
}

void MainWindow::timelineSelectionChanged(ResizeableItem* item)
{
    scene->clearSelection();
    item->setSelected(true);

    m_itemPropertyEditor->setItem(item);
    propertiesdock->setWidget(m_itemPropertyEditor);
}

void MainWindow::sceneItemAdded(QGraphicsItem *item)
{
    ResizeableItem *ri = dynamic_cast<ResizeableItem*>(item);
    QTreeWidgetItem *treeItem = new QTreeWidgetItem();
    treeItem->setText(0, ri->id());
    treeItem->setIcon(0, QIcon(":/images/rect.png"));
    treeItem->setData(0, 3, qVariantFromValue((void *) ri));
    connect(ri, SIGNAL(idChanged(ResizeableItem *, QString)), this, SLOT(idChanged(ResizeableItem *, QString)));

    root->addChild(treeItem);
    root->setExpanded(true);
    root->setSelected(false);
    for(int i=0; i<root->childCount(); i++)
        root->child(i)->setSelected(false);
    treeItem->setSelected(true);

    item->setSelected(true);
    selectAct->setChecked(true);
    scene->setEditMode(AnimationScene::EditMode::ModeSelect);
}

void MainWindow::showPropertyPanel()
{
    propertiesdock->setVisible(true);
}

void MainWindow::showToolPanel()
{
    tooldock->setVisible(true);
}

void MainWindow::showElementsPanel()
{
    elementsdock->setVisible(true);
}

void MainWindow::showNewsPanel()
{
    newsdock->setVisible(true);
}

void MainWindow::copy()
{
    scene->copyItem();
}

void MainWindow::paste()
{
    scene->pasteItem();
}

void MainWindow::del()
{
    while(scene->selectedItems().count())
    {
        ResizeableItem *item = dynamic_cast<ResizeableItem*>(scene->selectedItems().first());
        if(item)
            scene->deleteItem(item);
    }
}

void MainWindow::sceneItemRemoved(ResizeableItem *item)
{
    for(int i=0; i<root->childCount(); i++)
    {
        if(root->child(i)->data(0, 3).value<void *>() == item)
        {
            QTreeWidgetItem *treeItem = root->child(i);
            root->removeChild(treeItem);
            delete treeItem;
            break;
        }
    }
    timeline->removeItem(item);
}

void MainWindow::transitionSelectionChanged(KeyFrame *frame)
{
    if(frame)
    {
        scene->clearSelection();
        m_transitionEditor->setKeyframe(frame);
        propertiesdock->setWidget(m_transitionEditor);
    }
    else
        propertiesdock->setWidget(m_scenePropertyEditor);
}

void MainWindow::exportMovie()
{
    QString fileName;
    QFileDialog *dialog = new QFileDialog();
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter("Video format (*.mpg *.mp4 *.avi *.gif);;All Files (*)");
    dialog->setWindowTitle("Export Animation to Movie");
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    if(dialog->exec())
        fileName = dialog->selectedFiles().first();
    delete dialog;
    if(fileName.isEmpty())
        return;

    scene->clearSelection();
    view->setUpdatesEnabled(false);
    QGraphicsView *exportView = new QGraphicsView(scene);
    exportView->setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
    exportView->setVerticalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
    exportView->setGeometry(0,0,scene->width(), scene->height());

    int delay = 1000 / scene->fps();
    int frames = timeline->lastKeyframe() / delay + 2;

    QDir tmp = QDir::temp();
    tmp.mkdir("animationmaker");
    QString filterString = "frame%03d.png";
    tmp.cd("animationmaker");

    QFile list(tmp.absolutePath() + "/list");
    if(!list.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "Error", "Unable to create list file");
        return;
    }

    for (int i = 0; i < frames; i++)
    {
        statusBar()->showMessage(QString("Writing frame %1 of %2 frames").arg(i).arg(frames));

        timeline->setPlayheadPosition(i * delay);
        QTest::qSleep(delay);
        QCoreApplication::processEvents(QEventLoop::AllEvents, delay);

        QImage img = exportView->grab().toImage();
        QString imgName = tmp.absolutePath() + "/" + QString::asprintf(filterString.toLatin1(), i);
        img.save(imgName);
        QString entry = "file '" + QString::asprintf(filterString.toLatin1(), i) + "'\n";
        list.write(entry.toLatin1());
    }
    list.close();
    statusBar()->showMessage(QString("Creating movie file"));


    if(fileName.endsWith(".gif"))
    {
        QString output = tmp.absolutePath() + "/temp.mp4";
        statusBar()->showMessage("Creating temp movie");
        runCommand("\"" + qApp->applicationDirPath() + "/ffmpeg\" -r " + QString::number(scene->fps()) + " -safe 0 -f concat -i list -b 4M -y " + output, tmp.absolutePath());
        statusBar()->showMessage("Creating palette file");
        runCommand("\"" + qApp->applicationDirPath() + "/ffmpeg\" -i " + output + " -vf palettegen -y " + tmp.absolutePath() + "/temp.png", tmp.absolutePath());
        statusBar()->showMessage("Converting temp movie");
        runCommand("\"" + qApp->applicationDirPath() + "/ffmpeg\" -r " + QString::number(scene->fps()) + " -i " + output + " -i " + tmp.absolutePath() + "/temp.png -lavfi paletteuse -y " + fileName, tmp.absolutePath());
    }
    else
    {
        statusBar()->showMessage("Creating movie file");
        runCommand("\"" + qApp->applicationDirPath() + "/ffmpeg\" -r " + QString::number(scene->fps()) + " -safe 0 -f concat -i list -b 4M -y " + fileName, tmp.absolutePath());
    }

    tmp.removeRecursively();
    view->setUpdatesEnabled(true);
    statusBar()->showMessage("Ready");
    delete exportView;
}

void MainWindow::runCommand(QString cmd, QString path)
{
    QProcess *proc = new QProcess();
    proc->setWorkingDirectory(path);
    proc->start(cmd);
    proc->waitForFinished(-1);
    qDebug() << proc->readAllStandardOutput();
    qDebug() << proc->readAllStandardError();
    delete proc;
}

void MainWindow::importXml()
{
    scene->importXml();
    fillTree();
    elementTree->expandAll();
    m_scenePropertyEditor->setScene(scene);
    timeline->expandTree();
    statusBar()->showMessage(QString("Ready"));
}

void MainWindow::exportXml()
{
    scene->exportXml();
    statusBar()->showMessage(QString("Ready"));
}
