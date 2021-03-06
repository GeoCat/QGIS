/***************************************************************************
                         qgscomposermapwidget.h
                         ----------------------
    begin                : May 26, 2008
    copyright            : (C) 2008 by Marco Hugentobler
    email                : marco dot hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSCOMPOSERMAPWIDGET_H
#define QGSCOMPOSERMAPWIDGET_H

#include "ui_qgscomposermapwidgetbase.h"
#include "qgscomposeritemwidget.h"
#include "qgscomposermapgrid.h"

class QgsMapLayer;
class QgsComposerMapOverview;

/** \ingroup app
 * Input widget for the configuration of QgsComposerMap
 * */
class QgsComposerMapWidget: public QgsComposerItemBaseWidget, private Ui::QgsComposerMapWidgetBase
{
    Q_OBJECT

  public:
    explicit QgsComposerMapWidget( QgsComposerMap *composerMap );

  public slots:
    void on_mScaleLineEdit_editingFinished();
    void on_mSetToMapCanvasExtentButton_clicked();
    void on_mViewExtentInCanvasButton_clicked();
    void on_mUpdatePreviewButton_clicked();
    void on_mFollowVisibilityPresetCheckBox_stateChanged( int state );
    void on_mKeepLayerListCheckBox_stateChanged( int state );
    void on_mKeepLayerStylesCheckBox_stateChanged( int state );
    void on_mDrawCanvasItemsCheckBox_stateChanged( int state );
    void overviewMapChanged( QgsComposerItem *item );
    void on_mOverviewFrameStyleButton_clicked();
    void on_mOverviewBlendModeComboBox_currentIndexChanged( int index );
    void on_mOverviewInvertCheckbox_toggled( bool state );
    void on_mOverviewCenterCheckbox_toggled( bool state );

    void on_mXMinLineEdit_editingFinished();
    void on_mXMaxLineEdit_editingFinished();
    void on_mYMinLineEdit_editingFinished();
    void on_mYMaxLineEdit_editingFinished();

    void on_mAtlasMarginRadio_toggled( bool checked );

    void on_mAtlasCheckBox_toggled( bool checked );
    void on_mAtlasMarginSpinBox_valueChanged( int value );
    void on_mAtlasFixedScaleRadio_toggled( bool checked );
    void on_mAtlasPredefinedScaleRadio_toggled( bool checked );

    void on_mAddGridPushButton_clicked();
    void on_mRemoveGridPushButton_clicked();
    void on_mGridUpButton_clicked();
    void on_mGridDownButton_clicked();

    QgsComposerMapGrid *currentGrid();
    void on_mDrawGridCheckBox_toggled( bool state );
    void on_mGridListWidget_currentItemChanged( QListWidgetItem *current, QListWidgetItem *previous );
    void on_mGridListWidget_itemChanged( QListWidgetItem *item );
    void on_mGridPropertiesButton_clicked();

    //overviews
    void on_mAddOverviewPushButton_clicked();
    void on_mRemoveOverviewPushButton_clicked();
    void on_mOverviewUpButton_clicked();
    void on_mOverviewDownButton_clicked();
    QgsComposerMapOverview *currentOverview();
    void on_mOverviewCheckBox_toggled( bool state );
    void on_mOverviewListWidget_currentItemChanged( QListWidgetItem *current, QListWidgetItem *previous );
    void on_mOverviewListWidget_itemChanged( QListWidgetItem *item );
    void setOverviewItemsEnabled( bool enabled );
    void setOverviewItems( const QgsComposerMapOverview *overview );
    void blockOverviewItemsSignals( bool block );

  protected:

    void addPageToToolbox( QWidget *widget, const QString &name );

    //! Sets the current composer map values to the GUI elements
    virtual void updateGuiElements();

  protected slots:
    //! Initializes data defined buttons to current atlas coverage layer
    void populateDataDefinedButtons();

  private slots:

    //! Sets the GUI elements to the values of mPicture
    void setGuiElementValues();

    //! Enables or disables the atlas margin around feature option depending on coverage layer type
    void atlasLayerChanged( QgsVectorLayer *layer );

    //! Enables or disables the atlas controls when composer atlas is toggled on/off
    void compositionAtlasToggled( bool atlasEnabled );

    void aboutToShowKeepLayersVisibilityPresetsMenu();

    void followVisibilityPresetSelected( int currentIndex );
    void keepLayersVisibilityPresetSelected();

    void onMapThemesChanged();

    void updateOverviewFrameStyleFromWidget();
    void cleanUpOverviewFrameStyleSelector( QgsPanelWidget *container );

    void mapCrsChanged( const QgsCoordinateReferenceSystem &crs );

  private:
    QgsComposerMap *mComposerMap = nullptr;

    //! Sets extent of composer map from line edits
    void updateComposerExtentFromGui();

    //! Blocks / unblocks the signals of all GUI elements
    void blockAllSignals( bool b );

    void rotationChanged( double value );

    void handleChangedFrameDisplay( QgsComposerMapGrid::BorderSide border, const QgsComposerMapGrid::DisplayMode mode );
    void handleChangedAnnotationDisplay( QgsComposerMapGrid::BorderSide border, const QString &text );
    void handleChangedAnnotationPosition( QgsComposerMapGrid::BorderSide border, const QString &text );
    void handleChangedAnnotationDirection( QgsComposerMapGrid::BorderSide border, QgsComposerMapGrid::AnnotationDirection direction );

    void insertFrameDisplayEntries( QComboBox *c );
    void insertAnnotationDisplayEntries( QComboBox *c );
    void insertAnnotationPositionEntries( QComboBox *c );
    void insertAnnotationDirectionEntries( QComboBox *c );

    void initFrameDisplayBox( QComboBox *c, QgsComposerMapGrid::DisplayMode display );
    void initAnnotationDisplayBox( QComboBox *c, QgsComposerMapGrid::DisplayMode display );
    void initAnnotationPositionBox( QComboBox *c, QgsComposerMapGrid::AnnotationPosition pos );
    void initAnnotationDirectionBox( QComboBox *c, QgsComposerMapGrid::AnnotationDirection dir );

    //! Enables or disables the atlas margin and predefined scales radio depending on the atlas coverage layer type
    void toggleAtlasScalingOptionsByLayerType();

    //! Recalculates the bounds for an atlas map when atlas properties change
    void updateMapForAtlas();

    //! Is there some predefined scales, globally or as project's options ?
    bool hasPredefinedScales() const;

    QListWidgetItem *addGridListItem( const QString &id, const QString &name );

    void loadGridEntries();

    QListWidgetItem *addOverviewListItem( const QString &id, const QString &name );

    void loadOverviewEntries();

    void updateOverviewFrameSymbolMarker( const QgsComposerMapOverview *overview );

    void storeCurrentLayerSet();

};

#endif
