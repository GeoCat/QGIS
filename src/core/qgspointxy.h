/***************************************************************************
                          qgspoint.h  -  description
                             -------------------
    begin                : Sat Jun 22 2002
    copyright            : (C) 2002 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPOINT_H
#define QGSPOINT_H

#include "qgis_core.h"
#include "qgsvector.h"

#include "qgis.h"

#include <iostream>
#include <QString>
#include <QPoint>
#include <QObject>

class QgsPoint;

/** \ingroup core
 * A class to represent a 2D point.
 *
 * A QgsPointXY represents a position with X and Y coordinates.
 * In most scenarios it is preferable to use a QgsPoint instead which also
 * supports Z and M values.
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsPointXY
{
    Q_GADGET

    Q_PROPERTY( double x READ x WRITE setX )
    Q_PROPERTY( double y READ y WRITE setY )

  public:
    /// Default constructor
    QgsPointXY()
    {}

    //! Create a point from another point
    QgsPointXY( const QgsPointXY &p );

    /** Create a point from x,y coordinates
     * \param x x coordinate
     * \param y y coordinate
     */
    QgsPointXY( double x, double y )
      : mX( x )
      , mY( y )
    {}

    /** Create a point from a QPointF
     * \param point QPointF source
     * \since QGIS 2.7
     */
    QgsPointXY( QPointF point )
      : mX( point.x() )
      , mY( point.y() )
    {}

    /** Create a point from a QPoint
     * \param point QPoint source
     * \since QGIS 2.7
     */
    QgsPointXY( QPoint point )
      : mX( point.x() )
      , mY( point.y() )
    {}

    /**
     * Create a new point.
     * Z and M values will be dropped.
     *
     * \since QGIS 3.0
     */
    QgsPointXY( const QgsPoint &point );

    // IMPORTANT - while QgsPointXY is inherited by QgsReferencedPointXY, we do NOT want a virtual destructor here
    // because this class MUST be lightweight and we don't want the cost of the vtable here.
    // see https://github.com/qgis/QGIS/pull/4720#issuecomment-308652392
    ~QgsPointXY() = default;

    /** Sets the x value of the point
     * \param x x coordinate
     */
    void setX( double x )
    {
      mX = x;
    }

    /** Sets the y value of the point
     * \param y y coordinate
     */
    void setY( double y )
    {
      mY = y;
    }

    //! Sets the x and y value of the point
    void set( double x, double y )
    {
      mX = x;
      mY = y;
    }

    /** Get the x value of the point
     * \returns x coordinate
     */
    double x() const
    {
      return mX;
    }

    /** Get the y value of the point
     * \returns y coordinate
     */
    double y() const
    {
      return mY;
    }

    /** Converts a point to a QPointF
     * \returns QPointF with same x and y values
     * \since QGIS 2.7
     */
    QPointF toQPointF() const;

    //! String representation of the point (x,y)
    QString toString() const;

    //! As above but with precision for string representation of a point
    QString toString( int precision ) const;

    /** Return a string representation as degrees minutes seconds.
     *  Its up to the calling function to ensure that this point can
     *  be meaningfully represented in this form.
     *  \param precision number of decimal points to use for seconds
     *  \param useSuffix set to true to include a direction suffix (e.g., 'N'),
     *  set to false to use a "-" prefix for west and south coordinates
     *  \param padded set to true to force minutes and seconds to use two decimals,
     *  e.g., '05' instead of '5'.
     */
    QString toDegreesMinutesSeconds( int precision, const bool useSuffix = true, const bool padded = false ) const;

    /** Return a string representation as degrees minutes.
     *  Its up to the calling function to ensure that this point can
     *  be meaningfully represented in this form.
     *  \param precision number of decimal points to use for minutes
     *  \param useSuffix set to true to include a direction suffix (e.g., 'N'),
     *  set to false to use a "-" prefix for west and south coordinates
     *  \param padded set to true to force minutes to use two decimals,
     *  e.g., '05' instead of '5'.
     */
    QString toDegreesMinutes( int precision, const bool useSuffix = true, const bool padded = false ) const;


    /** Return the well known text representation for the point.
     * The wkt is created without an SRID.
     * \returns Well known text in the form POINT(x y)
     */
    QString wellKnownText() const;

    /** Returns the squared distance between this point a specified x, y coordinate.
     * \see distance()
    */
    double sqrDist( double x, double y ) const;

    /** Returns the squared distance between this point another point.
     * \see distance()
    */
    double sqrDist( const QgsPointXY &other ) const;

    /** Returns the distance between this point and a specified x, y coordinate.
     * \param x x-coordniate
     * \param y y-coordinate
     * \see sqrDist()
     * \since QGIS 2.16
    */
    double distance( double x, double y ) const;

    /** Returns the distance between this point and another point.
     * \param other other point
     * \see sqrDist()
     * \since QGIS 2.16
    */
    double distance( const QgsPointXY &other ) const;

    //! Returns the minimum distance between this point and a segment
    double sqrDistToSegment( double x1, double y1, double x2, double y2, QgsPointXY &minDistPoint SIP_OUT, double epsilon = DEFAULT_SEGMENT_EPSILON ) const;

    //! Calculates azimuth between this point and other one (clockwise in degree, starting from north)
    double azimuth( const QgsPointXY &other ) const;

    /** Returns a new point which corresponds to this point projected by a specified distance
     * in a specified bearing.
     * \param distance distance to project
     * \param bearing angle to project in, clockwise in degrees starting from north
     * \since QGIS 2.16
     */
    QgsPointXY project( double distance, double bearing ) const;

    /** Compares this point with another point with a fuzzy tolerance
     * \param other point to compare with
     * \param epsilon maximum difference for coordinates between the points
     * \returns true if points are equal within specified tolerance
     * \since QGIS 2.9
     */
    bool compare( const QgsPointXY &other, double epsilon = 4 * DBL_EPSILON ) const;

    //! equality operator
    bool operator==( const QgsPointXY &other );

    //! Inequality operator
    bool operator!=( const QgsPointXY &other ) const;

    //! Multiply x and y by the given value
    void multiply( double scalar );

    //! Assignment
    QgsPointXY &operator=( const QgsPointXY &other );

    //! Calculates the vector obtained by subtracting a point from this point
    QgsVector operator-( const QgsPointXY &p ) const { return QgsVector( mX - p.mX, mY - p.mY ); }

    //! Adds a vector to this point in place
    QgsPointXY &operator+=( QgsVector v ) { *this = *this + v; return *this; }

    //! Subtracts a vector from this point in place
    QgsPointXY &operator-=( QgsVector v ) { *this = *this - v; return *this; }

    //! Adds a vector to this point
    QgsPointXY operator+( QgsVector v ) const { return QgsPointXY( mX + v.x(), mY + v.y() ); }

    //! Subtracts a vector from this point
    QgsPointXY operator-( QgsVector v ) const { return QgsPointXY( mX - v.x(), mY - v.y() ); }

    //! Multiplies the coordinates in this point by a scalar quantity
    QgsPointXY operator*( double scalar ) const { return QgsPointXY( mX * scalar, mY * scalar ); }

    //! Divides the coordinates in this point by a scalar quantity
    QgsPointXY operator/( double scalar ) const { return QgsPointXY( mX / scalar, mY / scalar ); }

    //! Multiplies the coordinates in this point by a scalar quantity in place
    QgsPointXY &operator*=( double scalar ) { mX *= scalar; mY *= scalar; return *this; }

    //! Divides the coordinates in this point by a scalar quantity in place
    QgsPointXY &operator/=( double scalar ) { mX /= scalar; mY /= scalar; return *this; }

    //! Allows direct construction of QVariants from points.
    operator QVariant() const
    {
      return QVariant::fromValue( *this );
    }

#ifdef SIP_RUN
    SIP_PYOBJECT __repr__();
    % MethodCode
    QString str = "(" + QString::number( sipCpp->x() ) + "," + QString::number( sipCpp->y() ) + ")";
    //QString str("(%f,%f)").arg(sipCpp->x()).arg(sipCpp->y());
    sipRes = PyUnicode_FromString( str.toUtf8().data() );
    % End

    int __len__();
    % MethodCode
    sipRes = 2;
    % End


    SIP_PYOBJECT __getitem__( int );
    % MethodCode
    if ( a0 == 0 )
    {
      sipRes = Py_BuildValue( "d", sipCpp->x() );
    }
    else if ( a0 == 1 )
    {
      sipRes = Py_BuildValue( "d", sipCpp->y() );
    }
    else
    {
      QString msg = QString( "Bad index: %1" ).arg( a0 );
      PyErr_SetString( PyExc_IndexError, msg.toAscii().constData() );
    }
    % End

    long __hash__() const;
    % MethodCode
    sipRes = qHash( *sipCpp );
    % End
#endif

  private:

    //! x coordinate
    double mX = 0.0;

    //! y coordinate
    double mY = 0.0;

    friend uint qHash( const QgsPointXY &pnt );

}; // class QgsPoint

Q_DECLARE_METATYPE( QgsPointXY )

inline bool operator==( const QgsPointXY &p1, const QgsPointXY &p2 ) SIP_SKIP
{
  if ( qgsDoubleNear( p1.x(), p2.x() ) && qgsDoubleNear( p1.y(), p2.y() ) )
  { return true; }
  else
  { return false; }
}

inline std::ostream &operator << ( std::ostream &os, const QgsPointXY &p ) SIP_SKIP
{
  // Use Local8Bit for printouts
  os << p.toString().toLocal8Bit().data();
  return os;
}

inline uint qHash( const QgsPointXY &p ) SIP_SKIP
{
  uint hash;
  uint h1 = qHash( static_cast< quint64 >( p.mX ) );
  uint h2 = qHash( static_cast< quint64 >( p.mY ) );
  hash = h1 ^ ( h2 << 1 );
  return hash;
}


#endif //QGSPOINT_H
