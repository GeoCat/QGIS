/***************************************************************************
  qgscameracontroller.h
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSCAMERACONTROLLER_H
#define QGSCAMERACONTROLLER_H

#include "qgis_3d.h"

#include <Qt3DCore/QEntity>
#include <Qt3DInput>
#include <Qt3DRender>


/** \ingroup 3d
 * Object that controls camera movement based on user input
 * \since QGIS 3.0
 */
class _3D_EXPORT QgsCameraController : public Qt3DCore::QEntity
{
    Q_OBJECT
    Q_PROPERTY( Qt3DRender::QCamera *camera READ camera WRITE setCamera NOTIFY cameraChanged )
    Q_PROPERTY( QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged )
  public:
    //! Constructs the camera controller with optional parent node that will take ownership
    QgsCameraController( Qt3DCore::QNode *parent = nullptr );

    //! Returns camera that is being controlled
    Qt3DRender::QCamera *camera() const { return mCamera; }
    //! Returns viewport rectangle
    QRect viewport() const { return mViewport; }

    //! Connects to object picker attached to terrain entity. Called internally from 3D scene.
    //! This allows camera controller understand how far from the camera is the terrain under mouse cursor
    void addTerrainPicker( Qt3DRender::QObjectPicker *picker );
    //! Assigns camera that should be controlled by this class. Called internally from 3D scene.
    void setCamera( Qt3DRender::QCamera *camera );
    //! Sets viewport rectangle. Called internally from 3D canvas. Allows conversion of mouse coordinates.
    void setViewport( const QRect &viewport );
    //! Called internally from 3D scene when a new frame is generated. Updates camera according to keyboard/mouse input
    void frameTriggered( float dt );

    //! Move camera back to the initial position (looking down towards origin of world's coordinates)
    void resetView( float distance );

  private:
    void setCameraData( float x, float y, float dist, float pitch = 0, float yaw = 0 );

  signals:
    //! Emitted when camera has been updated
    void cameraChanged();
    //! Emitted when viewport rectangle has been updated
    void viewportChanged();

  private slots:
    void onPositionChanged( Qt3DInput::QMouseEvent *mouse );
    void onPickerMousePressed( Qt3DRender::QPickEvent *pick );

  private:
    //! Camera that is being controlled
    Qt3DRender::QCamera *mCamera;
    //! used for computation of translation when dragging mouse
    QRect mViewport;
    //! height of terrain when mouse button was last pressed - for camera control
    float mLastPressedHeight;

    struct CameraData
    {
      float x = 0, y = 0;  // ground point towards which the camera is looking
      float dist = 40;  // distance of camera from the point it is looking at
      float pitch = 0; // aircraft nose up/down (0 = looking straight down to the plane)
      float yaw = 0;   // aircraft nose left/right

      bool operator==( const CameraData &other ) const
      {
        return x == other.x && y == other.y && dist == other.dist && pitch == other.pitch && yaw == other.yaw;
      }
      bool operator!=( const CameraData &other ) const
      {
        return !operator==( other );
      }

      void setCamera( Qt3DRender::QCamera *camera )
      {
        // basic scene setup:
        // - x grows to the right
        // - z grows to the bottom
        // - y grows towards camera
        // so a point on the plane (x',y') is transformed to (x,-z) in our 3D world
        camera->setUpVector( QVector3D( 0, 0, -1 ) );
        camera->setPosition( QVector3D( x, dist, y ) );
        camera->setViewCenter( QVector3D( x, 0, y ) );
        camera->rotateAboutViewCenter( QQuaternion::fromEulerAngles( pitch, yaw, 0 ) );
      }

    };

    CameraData mCameraData;

    //! Last mouse position recorded
    QPoint mMousePos;
    //! Mouse position used in the previous frame
    QPoint mLastMousePos;

    //! Delegates mouse events to the attached MouseHandler objects
    Qt3DInput::QMouseDevice *mMouseDevice;

    Qt3DInput::QKeyboardDevice *mKeyboardDevice;

    Qt3DInput::QMouseHandler *mMouseHandler;

    //! Allows us to define a set of actions that we wish to use
    //! (it is a component that can be attached to 3D scene)
    Qt3DInput::QLogicalDevice *mLogicalDevice;

    Qt3DInput::QAction *mLeftMouseButtonAction;
    Qt3DInput::QActionInput *mLeftMouseButtonInput;

    Qt3DInput::QAction *mMiddleMouseButtonAction;
    Qt3DInput::QActionInput *mMiddleMouseButtonInput;

    Qt3DInput::QAction *mRightMouseButtonAction;
    Qt3DInput::QActionInput *mRightMouseButtonInput;

    Qt3DInput::QAction *mShiftAction;
    Qt3DInput::QActionInput *mShiftInput;

    Qt3DInput::QAxis *mWheelAxis;
    Qt3DInput::QAnalogAxisInput *mMouseWheelInput;

    Qt3DInput::QAxis *mTxAxis;
    Qt3DInput::QAxis *mTyAxis;
    Qt3DInput::QButtonAxisInput *mKeyboardTxPosInput;
    Qt3DInput::QButtonAxisInput *mKeyboardTyPosInput;
    Qt3DInput::QButtonAxisInput *mKeyboardTxNegInput;
    Qt3DInput::QButtonAxisInput *mKeyboardTyNegInput;
};

#endif // QGSCAMERACONTROLLER_H
