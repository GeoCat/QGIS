/***************************************************************************
                        qgsgeos.cpp
  -------------------------------------------------------------------
Date                 : 22 Sept 2014
Copyright            : (C) 2014 by Marco Hugentobler
email                : marco.hugentobler at sourcepole dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsgeos.h"
#include "qgsabstractgeometry.h"
#include "qgsgeometrycollection.h"
#include "qgsgeometryfactory.h"
#include "qgslinestring.h"
#include "qgsmessagelog.h"
#include "qgsmulticurve.h"
#include "qgsmultilinestring.h"
#include "qgsmultipoint.h"
#include "qgsmultipolygon.h"
#include "qgslogger.h"
#include "qgspolygon.h"
#include <limits>
#include <cstdio>

#define DEFAULT_QUADRANT_SEGMENTS 8

#define CATCH_GEOS(r) \
  catch (GEOSException &e) \
  { \
    QgsMessageLog::logMessage( QObject::tr( "Exception: %1" ).arg( e.what() ), QObject::tr("GEOS") ); \
    return r; \
  }

#define CATCH_GEOS_WITH_ERRMSG(r) \
  catch (GEOSException &e) \
  { \
    QgsMessageLog::logMessage( QObject::tr( "Exception: %1" ).arg( e.what() ), QObject::tr("GEOS") ); \
    if ( errorMsg ) \
    { \
      *errorMsg = e.what(); \
    } \
    return r; \
  }

/// @cond PRIVATE

static void throwGEOSException( const char *fmt, ... )
{
  va_list ap;
  char buffer[1024];

  va_start( ap, fmt );
  vsnprintf( buffer, sizeof buffer, fmt, ap );
  va_end( ap );

  QgsDebugMsg( QString( "GEOS exception: %1" ).arg( buffer ) );

  throw GEOSException( QString::fromUtf8( buffer ) );
}

static void printGEOSNotice( const char *fmt, ... )
{
#if defined(QGISDEBUG)
  va_list ap;
  char buffer[1024];

  va_start( ap, fmt );
  vsnprintf( buffer, sizeof buffer, fmt, ap );
  va_end( ap );

  QgsDebugMsg( QString( "GEOS notice: %1" ).arg( QString::fromUtf8( buffer ) ) );
#else
  Q_UNUSED( fmt );
#endif
}

class GEOSInit
{
  public:
    GEOSContextHandle_t ctxt;

    GEOSInit()
    {
      ctxt = initGEOS_r( printGEOSNotice, throwGEOSException );
    }

    ~GEOSInit()
    {
      finishGEOS_r( ctxt );
    }

    GEOSInit( const GEOSInit &rh ) = delete;
    GEOSInit &operator=( const GEOSInit &rh ) = delete;
};

static GEOSInit geosinit;

///@endcond


/** \ingroup core
 * @brief Scoped GEOS pointer
 * @note not available in Python bindings
 */
class GEOSGeomScopedPtr
{
  public:

    /**
     * Constructor for GEOSGeomScopedPtr, owning the specified \a geom.
     */
    explicit GEOSGeomScopedPtr( GEOSGeometry *geom = nullptr ) : mGeom( geom ) {}
    ~GEOSGeomScopedPtr() { GEOSGeom_destroy_r( geosinit.ctxt, mGeom ); }

    //! GEOSGeomScopedPtr cannot be copied
    GEOSGeomScopedPtr( const GEOSGeomScopedPtr &rh ) = delete;
    //! GEOSGeomScopedPtr cannot be copied
    GEOSGeomScopedPtr &operator=( const GEOSGeomScopedPtr &rh ) = delete;

    GEOSGeometry *get() const { return mGeom; }
    operator bool() const { return nullptr != mGeom; }
    void reset( GEOSGeometry *geom )
    {
      GEOSGeom_destroy_r( geosinit.ctxt, mGeom );
      mGeom = geom;
    }

  private:
    GEOSGeometry *mGeom = nullptr;

};

QgsGeos::QgsGeos( const QgsAbstractGeometry *geometry, double precision )
  : QgsGeometryEngine( geometry )
  , mPrecision( precision )
{
  cacheGeos();
}

QgsGeos::~QgsGeos()
{
  GEOSGeom_destroy_r( geosinit.ctxt, mGeos );
  mGeos = nullptr;
  GEOSPreparedGeom_destroy_r( geosinit.ctxt, mGeosPrepared );
  mGeosPrepared = nullptr;
}

void QgsGeos::geometryChanged()
{
  GEOSGeom_destroy_r( geosinit.ctxt, mGeos );
  mGeos = nullptr;
  GEOSPreparedGeom_destroy_r( geosinit.ctxt, mGeosPrepared );
  mGeosPrepared = nullptr;
  cacheGeos();
}

void QgsGeos::prepareGeometry()
{
  GEOSPreparedGeom_destroy_r( geosinit.ctxt, mGeosPrepared );
  mGeosPrepared = nullptr;
  if ( mGeos )
  {
    mGeosPrepared = GEOSPrepare_r( geosinit.ctxt, mGeos );
  }
}

void QgsGeos::cacheGeos() const
{
  if ( !mGeometry || mGeos )
  {
    return;
  }

  mGeos = asGeos( mGeometry, mPrecision );
}

QgsAbstractGeometry *QgsGeos::intersection( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return overlay( geom, INTERSECTION, errorMsg );
}

QgsAbstractGeometry *QgsGeos::difference( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return overlay( geom, DIFFERENCE, errorMsg );
}

QgsAbstractGeometry *QgsGeos::clip( const QgsRectangle &rect, QString *errorMsg ) const
{
  if ( !mGeos || rect.isNull() || rect.isEmpty() )
  {
    return nullptr;
  }

  try
  {
    GEOSGeomScopedPtr opGeom;
    opGeom.reset( GEOSClipByRect_r( geosinit.ctxt, mGeos, rect.xMinimum(), rect.yMinimum(), rect.xMaximum(), rect.yMaximum() ) );
    QgsAbstractGeometry *opResult = fromGeos( opGeom.get() );
    return opResult;
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return nullptr;
  }
}




void QgsGeos::subdivideRecursive( const GEOSGeometry *currentPart, int maxNodes, int depth, QgsGeometryCollection *parts, const QgsRectangle &clipRect ) const
{
  int partType = GEOSGeomTypeId_r( geosinit.ctxt, currentPart );
  if ( qgsDoubleNear( clipRect.width(), 0.0 ) && qgsDoubleNear( clipRect.height(), 0.0 ) )
  {
    if ( partType == GEOS_POINT )
    {
      parts->addGeometry( fromGeos( currentPart ) );
      return;
    }
    else
    {
      return;
    }
  }

  if ( partType == GEOS_MULTILINESTRING || partType == GEOS_MULTIPOLYGON || partType == GEOS_GEOMETRYCOLLECTION )
  {
    int partCount = GEOSGetNumGeometries_r( geosinit.ctxt, currentPart );
    for ( int i = 0; i < partCount; ++i )
    {
      subdivideRecursive( GEOSGetGeometryN_r( geosinit.ctxt, currentPart, i ), maxNodes, depth, parts, clipRect );
    }
    return;
  }

  if ( depth > 50 )
  {
    parts->addGeometry( fromGeos( currentPart ) );
    return;
  }

  int vertexCount = GEOSGetNumCoordinates_r( geosinit.ctxt, currentPart );
  if ( vertexCount == 0 )
  {
    return;
  }
  else if ( vertexCount < maxNodes )
  {
    parts->addGeometry( fromGeos( currentPart ) );
    return;
  }

  // chop clipping rect in half by longest side
  double width = clipRect.width();
  double height = clipRect.height();
  QgsRectangle halfClipRect1 = clipRect;
  QgsRectangle halfClipRect2 = clipRect;
  if ( width > height )
  {
    halfClipRect1.setXMaximum( clipRect.xMinimum() + width / 2.0 );
    halfClipRect2.setXMinimum( halfClipRect1.xMaximum() );
  }
  else
  {
    halfClipRect1.setYMaximum( clipRect.yMinimum() + height / 2.0 );
    halfClipRect2.setYMinimum( halfClipRect1.yMaximum() );
  }

  if ( height <= 0 )
  {
    halfClipRect1.setYMinimum( halfClipRect1.yMinimum() - DBL_EPSILON );
    halfClipRect2.setYMinimum( halfClipRect2.yMinimum() - DBL_EPSILON );
    halfClipRect1.setYMaximum( halfClipRect1.yMaximum() + DBL_EPSILON );
    halfClipRect2.setYMaximum( halfClipRect2.yMaximum() + DBL_EPSILON );
  }
  if ( width <= 0 )
  {
    halfClipRect1.setXMinimum( halfClipRect1.xMinimum() - DBL_EPSILON );
    halfClipRect2.setXMinimum( halfClipRect2.xMinimum() - DBL_EPSILON );
    halfClipRect1.setXMaximum( halfClipRect1.xMaximum() + DBL_EPSILON );
    halfClipRect2.setXMaximum( halfClipRect2.xMaximum() + DBL_EPSILON );
  }

  GEOSGeomScopedPtr clipPart1;
  clipPart1.reset( GEOSClipByRect_r( geosinit.ctxt, currentPart, halfClipRect1.xMinimum(), halfClipRect1.yMinimum(), halfClipRect1.xMaximum(), halfClipRect1.yMaximum() ) );
  GEOSGeomScopedPtr clipPart2;
  clipPart2.reset( GEOSClipByRect_r( geosinit.ctxt, currentPart, halfClipRect2.xMinimum(), halfClipRect2.yMinimum(), halfClipRect2.xMaximum(), halfClipRect2.yMaximum() ) );

  ++depth;

  if ( clipPart1 )
  {
    subdivideRecursive( clipPart1.get(), maxNodes, depth, parts, halfClipRect1 );
  }
  if ( clipPart2 )
  {
    subdivideRecursive( clipPart2.get(), maxNodes, depth, parts, halfClipRect2 );
  }
}

QgsAbstractGeometry *QgsGeos::subdivide( int maxNodes, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  // minimum allowed max is 8
  maxNodes = std::max( maxNodes, 8 );

  std::unique_ptr< QgsGeometryCollection > parts = QgsGeometryFactory::createCollectionOfType( mGeometry->wkbType() );
  try
  {
    subdivideRecursive( mGeos, maxNodes, 0, parts.get(), mGeometry->boundingBox() );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr )

  return parts.release();
}

QgsAbstractGeometry *QgsGeos::combine( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return overlay( geom, UNION, errorMsg );
}

QgsAbstractGeometry *QgsGeos::combine( const QList<QgsAbstractGeometry *> &geomList, QString *errorMsg ) const
{

  QVector< GEOSGeometry * > geosGeometries;
  geosGeometries.resize( geomList.size() );
  for ( int i = 0; i < geomList.size(); ++i )
  {
    geosGeometries[i] = asGeos( geomList.at( i ), mPrecision );
  }

  GEOSGeometry *geomUnion = nullptr;
  try
  {
    GEOSGeometry *geomCollection = createGeosCollection( GEOS_GEOMETRYCOLLECTION, geosGeometries );
    geomUnion = GEOSUnaryUnion_r( geosinit.ctxt, geomCollection );
    GEOSGeom_destroy_r( geosinit.ctxt, geomCollection );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr )

  QgsAbstractGeometry *result = fromGeos( geomUnion );
  GEOSGeom_destroy_r( geosinit.ctxt, geomUnion );
  return result;
}

QgsAbstractGeometry *QgsGeos::symDifference( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return overlay( geom, SYMDIFFERENCE, errorMsg );
}

double QgsGeos::distance( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  double distance = -1.0;
  if ( !mGeos )
  {
    return distance;
  }

  GEOSGeomScopedPtr otherGeosGeom( asGeos( geom, mPrecision ) );
  if ( !otherGeosGeom )
  {
    return distance;
  }

  try
  {
    GEOSDistance_r( geosinit.ctxt, mGeos, otherGeosGeom.get(), &distance );
  }
  CATCH_GEOS_WITH_ERRMSG( -1.0 )

  return distance;
}

double QgsGeos::hausdorffDistance( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  double distance = -1.0;
  if ( !mGeos )
  {
    return distance;
  }

  GEOSGeomScopedPtr otherGeosGeom( asGeos( geom, mPrecision ) );
  if ( !otherGeosGeom )
  {
    return distance;
  }

  try
  {
    GEOSHausdorffDistance_r( geosinit.ctxt, mGeos, otherGeosGeom.get(), &distance );
  }
  CATCH_GEOS_WITH_ERRMSG( -1.0 )

  return distance;
}

double QgsGeos::hausdorffDistanceDensify( const QgsAbstractGeometry *geom, double densifyFraction, QString *errorMsg ) const
{
  double distance = -1.0;
  if ( !mGeos )
  {
    return distance;
  }

  GEOSGeomScopedPtr otherGeosGeom( asGeos( geom, mPrecision ) );
  if ( !otherGeosGeom )
  {
    return distance;
  }

  try
  {
    GEOSHausdorffDistanceDensify_r( geosinit.ctxt, mGeos, otherGeosGeom.get(), densifyFraction, &distance );
  }
  CATCH_GEOS_WITH_ERRMSG( -1.0 )

  return distance;
}

bool QgsGeos::intersects( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, INTERSECTS, errorMsg );
}

bool QgsGeos::touches( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, TOUCHES, errorMsg );
}

bool QgsGeos::crosses( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, CROSSES, errorMsg );
}

bool QgsGeos::within( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, WITHIN, errorMsg );
}

bool QgsGeos::overlaps( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, OVERLAPS, errorMsg );
}

bool QgsGeos::contains( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, CONTAINS, errorMsg );
}

bool QgsGeos::disjoint( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  return relation( geom, DISJOINT, errorMsg );
}

QString QgsGeos::relate( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return QString();
  }

  GEOSGeomScopedPtr geosGeom( asGeos( geom, mPrecision ) );
  if ( !geosGeom )
  {
    return QString();
  }

  QString result;
  try
  {
    char *r = GEOSRelate_r( geosinit.ctxt, mGeos, geosGeom.get() );
    if ( r )
    {
      result = QString( r );
      GEOSFree_r( geosinit.ctxt, r );
    }
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
  }

  return result;
}

bool QgsGeos::relatePattern( const QgsAbstractGeometry *geom, const QString &pattern, QString *errorMsg ) const
{
  if ( !mGeos || !geom )
  {
    return false;
  }

  GEOSGeomScopedPtr geosGeom( asGeos( geom, mPrecision ) );
  if ( !geosGeom )
  {
    return false;
  }

  bool result = false;
  try
  {
    result = ( GEOSRelatePattern_r( geosinit.ctxt, mGeos, geosGeom.get(), pattern.toLocal8Bit().constData() ) == 1 );
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
  }

  return result;
}

double QgsGeos::area( QString *errorMsg ) const
{
  double area = -1.0;
  if ( !mGeos )
  {
    return area;
  }

  try
  {
    if ( GEOSArea_r( geosinit.ctxt, mGeos, &area ) != 1 )
      return -1.0;
  }
  CATCH_GEOS_WITH_ERRMSG( -1.0 );
  return area;
}

double QgsGeos::length( QString *errorMsg ) const
{
  double length = -1.0;
  if ( !mGeos )
  {
    return length;
  }
  try
  {
    if ( GEOSLength_r( geosinit.ctxt, mGeos, &length ) != 1 )
      return -1.0;
  }
  CATCH_GEOS_WITH_ERRMSG( -1.0 )
  return length;
}

QgsGeometryEngine::EngineOperationResult QgsGeos::splitGeometry( const QgsLineString &splitLine,
    QList<QgsAbstractGeometry *> &newGeometries,
    bool topological,
    QgsPointSequence &topologyTestPoints,
    QString *errorMsg ) const
{

  EngineOperationResult returnCode = Success;
  if ( !mGeos || !mGeometry )
  {
    return InvalidBaseGeometry;
  }

  //return if this type is point/multipoint
  if ( mGeometry->dimension() == 0 )
  {
    return SplitCannotSplitPoint; //cannot split points
  }

  if ( !GEOSisValid_r( geosinit.ctxt, mGeos ) )
    return InvalidBaseGeometry;

  //make sure splitLine is valid
  if ( ( mGeometry->dimension() == 1 && splitLine.numPoints() < 1 ) ||
       ( mGeometry->dimension() == 2 && splitLine.numPoints() < 2 ) )
    return InvalidInput;

  newGeometries.clear();
  GEOSGeometry *splitLineGeos = nullptr;

  try
  {
    if ( splitLine.numPoints() > 1 )
    {
      splitLineGeos = createGeosLinestring( &splitLine, mPrecision );
    }
    else if ( splitLine.numPoints() == 1 )
    {
      splitLineGeos = createGeosPointXY( splitLine.xAt( 0 ), splitLine.yAt( 0 ), false, 0, false, 0, 2, mPrecision );
    }
    else
    {
      return InvalidInput;
    }

    if ( !GEOSisValid_r( geosinit.ctxt, splitLineGeos ) || !GEOSisSimple_r( geosinit.ctxt, splitLineGeos ) )
    {
      GEOSGeom_destroy_r( geosinit.ctxt, splitLineGeos );
      return InvalidInput;
    }

    if ( topological )
    {
      //find out candidate points for topological corrections
      if ( !topologicalTestPointsSplit( splitLineGeos, topologyTestPoints ) )
      {
        return InvalidInput; // TODO: is it really an invalid input?
      }
    }

    //call split function depending on geometry type
    if ( mGeometry->dimension() == 1 )
    {
      returnCode = splitLinearGeometry( splitLineGeos, newGeometries );
      GEOSGeom_destroy_r( geosinit.ctxt, splitLineGeos );
    }
    else if ( mGeometry->dimension() == 2 )
    {
      returnCode = splitPolygonGeometry( splitLineGeos, newGeometries );
      GEOSGeom_destroy_r( geosinit.ctxt, splitLineGeos );
    }
    else
    {
      return InvalidInput;
    }
  }
  CATCH_GEOS_WITH_ERRMSG( EngineError )

  return returnCode;
}



bool QgsGeos::topologicalTestPointsSplit( const GEOSGeometry *splitLine, QgsPointSequence &testPoints, QString *errorMsg ) const
{
  //Find out the intersection points between splitLineGeos and this geometry.
  //These points need to be tested for topological correctness by the calling function
  //if topological editing is enabled

  if ( !mGeos )
  {
    return false;
  }

  try
  {
    testPoints.clear();
    GEOSGeometry *intersectionGeom = GEOSIntersection_r( geosinit.ctxt, mGeos, splitLine );
    if ( !intersectionGeom )
      return false;

    bool simple = false;
    int nIntersectGeoms = 1;
    if ( GEOSGeomTypeId_r( geosinit.ctxt, intersectionGeom ) == GEOS_LINESTRING
         || GEOSGeomTypeId_r( geosinit.ctxt, intersectionGeom ) == GEOS_POINT )
      simple = true;

    if ( !simple )
      nIntersectGeoms = GEOSGetNumGeometries_r( geosinit.ctxt, intersectionGeom );

    for ( int i = 0; i < nIntersectGeoms; ++i )
    {
      const GEOSGeometry *currentIntersectGeom = nullptr;
      if ( simple )
        currentIntersectGeom = intersectionGeom;
      else
        currentIntersectGeom = GEOSGetGeometryN_r( geosinit.ctxt, intersectionGeom, i );

      const GEOSCoordSequence *lineSequence = GEOSGeom_getCoordSeq_r( geosinit.ctxt, currentIntersectGeom );
      unsigned int sequenceSize = 0;
      double x, y;
      if ( GEOSCoordSeq_getSize_r( geosinit.ctxt, lineSequence, &sequenceSize ) != 0 )
      {
        for ( unsigned int i = 0; i < sequenceSize; ++i )
        {
          if ( GEOSCoordSeq_getX_r( geosinit.ctxt, lineSequence, i, &x ) != 0 )
          {
            if ( GEOSCoordSeq_getY_r( geosinit.ctxt, lineSequence, i, &y ) != 0 )
            {
              testPoints.push_back( QgsPoint( x, y ) );
            }
          }
        }
      }
    }
    GEOSGeom_destroy_r( geosinit.ctxt, intersectionGeom );
  }
  CATCH_GEOS_WITH_ERRMSG( true )

  return true;
}

GEOSGeometry *QgsGeos::linePointDifference( GEOSGeometry *GEOSsplitPoint ) const
{
  int type = GEOSGeomTypeId_r( geosinit.ctxt, mGeos );

  std::unique_ptr< QgsMultiCurve > multiCurve;
  if ( type == GEOS_MULTILINESTRING )
  {
    multiCurve.reset( qgsgeometry_cast<QgsMultiCurve *>( mGeometry->clone() ) );
  }
  else if ( type == GEOS_LINESTRING )
  {
    multiCurve.reset( new QgsMultiCurve() );
    multiCurve->addGeometry( mGeometry->clone() );
  }
  else
  {
    return nullptr;
  }

  if ( !multiCurve )
  {
    return nullptr;
  }


  std::unique_ptr< QgsAbstractGeometry > splitGeom( fromGeos( GEOSsplitPoint ) );
  QgsPoint *splitPoint = qgsgeometry_cast<QgsPoint *>( splitGeom.get() );
  if ( !splitPoint )
  {
    return nullptr;
  }

  QgsMultiCurve lines;

  //For each part
  for ( int i = 0; i < multiCurve->numGeometries(); ++i )
  {
    const QgsLineString *line = qgsgeometry_cast<const QgsLineString *>( multiCurve->geometryN( i ) );
    if ( line )
    {
      //For each segment
      QgsLineString newLine;
      newLine.addVertex( line->pointN( 0 ) );
      int nVertices = line->numPoints();
      for ( int j = 1; j < ( nVertices - 1 ); ++j )
      {
        QgsPoint currentPoint = line->pointN( j );
        newLine.addVertex( currentPoint );
        if ( currentPoint == *splitPoint )
        {
          lines.addGeometry( newLine.clone() );
          newLine = QgsLineString();
          newLine.addVertex( currentPoint );
        }
      }
      newLine.addVertex( line->pointN( nVertices - 1 ) );
      lines.addGeometry( newLine.clone() );
    }
  }

  return asGeos( &lines, mPrecision );
}

QgsGeometryEngine::EngineOperationResult QgsGeos::splitLinearGeometry( GEOSGeometry *splitLine, QList<QgsAbstractGeometry *> &newGeometries ) const
{
  if ( !splitLine )
    return InvalidInput;

  if ( !mGeos )
    return InvalidBaseGeometry;

  //first test if linestring intersects geometry. If not, return straight away
  if ( !GEOSIntersects_r( geosinit.ctxt, splitLine, mGeos ) )
    return NothingHappened;

  //check that split line has no linear intersection
  int linearIntersect = GEOSRelatePattern_r( geosinit.ctxt, mGeos, splitLine, "1********" );
  if ( linearIntersect > 0 )
    return InvalidInput;

  int splitGeomType = GEOSGeomTypeId_r( geosinit.ctxt, splitLine );

  GEOSGeometry *splitGeom = nullptr;
  if ( splitGeomType == GEOS_POINT )
  {
    splitGeom = linePointDifference( splitLine );
  }
  else
  {
    splitGeom = GEOSDifference_r( geosinit.ctxt, mGeos, splitLine );
  }
  QVector<GEOSGeometry *> lineGeoms;

  int splitType = GEOSGeomTypeId_r( geosinit.ctxt, splitGeom );
  if ( splitType == GEOS_MULTILINESTRING )
  {
    int nGeoms = GEOSGetNumGeometries_r( geosinit.ctxt, splitGeom );
    lineGeoms.reserve( nGeoms );
    for ( int i = 0; i < nGeoms; ++i )
      lineGeoms << GEOSGeom_clone_r( geosinit.ctxt, GEOSGetGeometryN_r( geosinit.ctxt, splitGeom, i ) );

  }
  else
  {
    lineGeoms << GEOSGeom_clone_r( geosinit.ctxt, splitGeom );
  }

  mergeGeometriesMultiTypeSplit( lineGeoms );

  for ( int i = 0; i < lineGeoms.size(); ++i )
  {
    newGeometries << fromGeos( lineGeoms[i] );
    GEOSGeom_destroy_r( geosinit.ctxt, lineGeoms[i] );
  }

  GEOSGeom_destroy_r( geosinit.ctxt, splitGeom );
  return Success;
}

QgsGeometryEngine::EngineOperationResult QgsGeos::splitPolygonGeometry( GEOSGeometry *splitLine, QList<QgsAbstractGeometry *> &newGeometries ) const
{
  if ( !splitLine )
    return InvalidInput;

  if ( !mGeos )
    return InvalidBaseGeometry;

  //first test if linestring intersects geometry. If not, return straight away
  if ( !GEOSIntersects_r( geosinit.ctxt, splitLine, mGeos ) )
    return NothingHappened;

  //first union all the polygon rings together (to get them noded, see JTS developer guide)
  GEOSGeometry *nodedGeometry = nodeGeometries( splitLine, mGeos );
  if ( !nodedGeometry )
    return NodedGeometryError; //an error occurred during noding

  GEOSGeometry *polygons = GEOSPolygonize_r( geosinit.ctxt, &nodedGeometry, 1 );
  if ( !polygons || numberOfGeometries( polygons ) == 0 )
  {
    if ( polygons )
      GEOSGeom_destroy_r( geosinit.ctxt, polygons );

    GEOSGeom_destroy_r( geosinit.ctxt, nodedGeometry );

    return InvalidBaseGeometry;
  }

  GEOSGeom_destroy_r( geosinit.ctxt, nodedGeometry );

  //test every polygon if contained in original geometry
  //include in result if yes
  QVector<GEOSGeometry *> testedGeometries;
  GEOSGeometry *intersectGeometry = nullptr;

  //ratio intersect geometry / geometry. This should be close to 1
  //if the polygon belongs to the input geometry

  for ( int i = 0; i < numberOfGeometries( polygons ); i++ )
  {
    const GEOSGeometry *polygon = GEOSGetGeometryN_r( geosinit.ctxt, polygons, i );
    intersectGeometry = GEOSIntersection_r( geosinit.ctxt, mGeos, polygon );
    if ( !intersectGeometry )
    {
      QgsDebugMsg( "intersectGeometry is nullptr" );
      continue;
    }

    double intersectionArea;
    GEOSArea_r( geosinit.ctxt, intersectGeometry, &intersectionArea );

    double polygonArea;
    GEOSArea_r( geosinit.ctxt, polygon, &polygonArea );

    const double areaRatio = intersectionArea / polygonArea;
    if ( areaRatio > 0.99 && areaRatio < 1.01 )
      testedGeometries << GEOSGeom_clone_r( geosinit.ctxt, polygon );

    GEOSGeom_destroy_r( geosinit.ctxt, intersectGeometry );
  }
  GEOSGeom_destroy_r( geosinit.ctxt, polygons );

  bool splitDone = true;
  int nGeometriesThis = numberOfGeometries( mGeos ); //original number of geometries
  if ( testedGeometries.size() == nGeometriesThis )
  {
    splitDone = false;
  }

  mergeGeometriesMultiTypeSplit( testedGeometries );

  //no split done, preserve original geometry
  if ( !splitDone )
  {
    for ( int i = 0; i < testedGeometries.size(); ++i )
    {
      GEOSGeom_destroy_r( geosinit.ctxt, testedGeometries[i] );
    }
    return NothingHappened;
  }

  int i;
  for ( i = 0; i < testedGeometries.size() && GEOSisValid_r( geosinit.ctxt, testedGeometries[i] ); ++i )
    ;

  if ( i < testedGeometries.size() )
  {
    for ( i = 0; i < testedGeometries.size(); ++i )
      GEOSGeom_destroy_r( geosinit.ctxt, testedGeometries[i] );

    return InvalidBaseGeometry;
  }

  for ( i = 0; i < testedGeometries.size(); ++i )
    newGeometries << fromGeos( testedGeometries[i] );

  return Success;
}

GEOSGeometry *QgsGeos::nodeGeometries( const GEOSGeometry *splitLine, const GEOSGeometry *geom )
{
  if ( !splitLine || !geom )
    return nullptr;

  GEOSGeometry *geometryBoundary = nullptr;
  if ( GEOSGeomTypeId_r( geosinit.ctxt, geom ) == GEOS_POLYGON || GEOSGeomTypeId_r( geosinit.ctxt, geom ) == GEOS_MULTIPOLYGON )
    geometryBoundary = GEOSBoundary_r( geosinit.ctxt, geom );
  else
    geometryBoundary = GEOSGeom_clone_r( geosinit.ctxt, geom );

  GEOSGeometry *splitLineClone = GEOSGeom_clone_r( geosinit.ctxt, splitLine );
  GEOSGeometry *unionGeometry = GEOSUnion_r( geosinit.ctxt, splitLineClone, geometryBoundary );
  GEOSGeom_destroy_r( geosinit.ctxt, splitLineClone );

  GEOSGeom_destroy_r( geosinit.ctxt, geometryBoundary );
  return unionGeometry;
}

int QgsGeos::mergeGeometriesMultiTypeSplit( QVector<GEOSGeometry *> &splitResult ) const
{
  if ( !mGeos )
    return 1;

  //convert mGeos to geometry collection
  int type = GEOSGeomTypeId_r( geosinit.ctxt, mGeos );
  if ( type != GEOS_GEOMETRYCOLLECTION &&
       type != GEOS_MULTILINESTRING &&
       type != GEOS_MULTIPOLYGON &&
       type != GEOS_MULTIPOINT )
    return 0;

  QVector<GEOSGeometry *> copyList = splitResult;
  splitResult.clear();

  //collect all the geometries that belong to the initial multifeature
  QVector<GEOSGeometry *> unionGeom;

  for ( int i = 0; i < copyList.size(); ++i )
  {
    //is this geometry a part of the original multitype?
    bool isPart = false;
    for ( int j = 0; j < GEOSGetNumGeometries_r( geosinit.ctxt, mGeos ); j++ )
    {
      if ( GEOSEquals_r( geosinit.ctxt, copyList[i], GEOSGetGeometryN_r( geosinit.ctxt, mGeos, j ) ) )
      {
        isPart = true;
        break;
      }
    }

    if ( isPart )
    {
      unionGeom << copyList[i];
    }
    else
    {
      QVector<GEOSGeometry *> geomVector;
      geomVector << copyList[i];

      if ( type == GEOS_MULTILINESTRING )
        splitResult << createGeosCollection( GEOS_MULTILINESTRING, geomVector );
      else if ( type == GEOS_MULTIPOLYGON )
        splitResult << createGeosCollection( GEOS_MULTIPOLYGON, geomVector );
      else
        GEOSGeom_destroy_r( geosinit.ctxt, copyList[i] );
    }
  }

  //make multifeature out of unionGeom
  if ( !unionGeom.isEmpty() )
  {
    if ( type == GEOS_MULTILINESTRING )
      splitResult << createGeosCollection( GEOS_MULTILINESTRING, unionGeom );
    else if ( type == GEOS_MULTIPOLYGON )
      splitResult << createGeosCollection( GEOS_MULTIPOLYGON, unionGeom );
  }
  else
  {
    unionGeom.clear();
  }

  return 0;
}

GEOSGeometry *QgsGeos::createGeosCollection( int typeId, const QVector<GEOSGeometry *> &geoms )
{
  int nNullGeoms = geoms.count( nullptr );
  int nNotNullGeoms = geoms.size() - nNullGeoms;

  GEOSGeometry **geomarr = new GEOSGeometry*[ nNotNullGeoms ];
  if ( !geomarr )
  {
    return nullptr;
  }

  int i = 0;
  QVector<GEOSGeometry *>::const_iterator geomIt = geoms.constBegin();
  for ( ; geomIt != geoms.constEnd(); ++geomIt )
  {
    if ( *geomIt )
    {
      geomarr[i] = *geomIt;
      ++i;
    }
  }
  GEOSGeometry *geom = nullptr;

  try
  {
    geom = GEOSGeom_createCollection_r( geosinit.ctxt, typeId, geomarr, nNotNullGeoms );
  }
  catch ( GEOSException &e )
  {
    QgsMessageLog::logMessage( QObject::tr( "Exception: %1" ).arg( e.what() ), QObject::tr( "GEOS" ) );
  }

  delete [] geomarr;

  return geom;
}

QgsAbstractGeometry *QgsGeos::fromGeos( const GEOSGeometry *geos )
{
  if ( !geos )
  {
    return nullptr;
  }

  int nCoordDims = GEOSGeom_getCoordinateDimension_r( geosinit.ctxt, geos );
  int nDims = GEOSGeom_getDimensions_r( geosinit.ctxt, geos );
  bool hasZ = ( nCoordDims == 3 );
  bool hasM = ( ( nDims - nCoordDims ) == 1 );

  switch ( GEOSGeomTypeId_r( geosinit.ctxt, geos ) )
  {
    case GEOS_POINT:                 // a point
    {
      const GEOSCoordSequence *cs = GEOSGeom_getCoordSeq_r( geosinit.ctxt, geos );
      return ( coordSeqPoint( cs, 0, hasZ, hasM ).clone() );
    }
    case GEOS_LINESTRING:
    {
      return sequenceToLinestring( geos, hasZ, hasM );
    }
    case GEOS_POLYGON:
    {
      return fromGeosPolygon( geos );
    }
    case GEOS_MULTIPOINT:
    {
      QgsMultiPointV2 *multiPoint = new QgsMultiPointV2();
      int nParts = GEOSGetNumGeometries_r( geosinit.ctxt, geos );
      for ( int i = 0; i < nParts; ++i )
      {
        const GEOSCoordSequence *cs = GEOSGeom_getCoordSeq_r( geosinit.ctxt, GEOSGetGeometryN_r( geosinit.ctxt, geos, i ) );
        if ( cs )
        {
          multiPoint->addGeometry( coordSeqPoint( cs, 0, hasZ, hasM ).clone() );
        }
      }
      return multiPoint;
    }
    case GEOS_MULTILINESTRING:
    {
      QgsMultiLineString *multiLineString = new QgsMultiLineString();
      int nParts = GEOSGetNumGeometries_r( geosinit.ctxt, geos );
      for ( int i = 0; i < nParts; ++i )
      {
        QgsLineString *line = sequenceToLinestring( GEOSGetGeometryN_r( geosinit.ctxt, geos, i ), hasZ, hasM );
        if ( line )
        {
          multiLineString->addGeometry( line );
        }
      }
      return multiLineString;
    }
    case GEOS_MULTIPOLYGON:
    {
      QgsMultiPolygonV2 *multiPolygon = new QgsMultiPolygonV2();

      int nParts = GEOSGetNumGeometries_r( geosinit.ctxt, geos );
      for ( int i = 0; i < nParts; ++i )
      {
        QgsPolygonV2 *poly = fromGeosPolygon( GEOSGetGeometryN_r( geosinit.ctxt, geos, i ) );
        if ( poly )
        {
          multiPolygon->addGeometry( poly );
        }
      }
      return multiPolygon;
    }
    case GEOS_GEOMETRYCOLLECTION:
    {
      QgsGeometryCollection *geomCollection = new QgsGeometryCollection();
      int nParts = GEOSGetNumGeometries_r( geosinit.ctxt, geos );
      for ( int i = 0; i < nParts; ++i )
      {
        QgsAbstractGeometry *geom = fromGeos( GEOSGetGeometryN_r( geosinit.ctxt, geos, i ) );
        if ( geom )
        {
          geomCollection->addGeometry( geom );
        }
      }
      return geomCollection;
    }
  }
  return nullptr;
}

QgsPolygonV2 *QgsGeos::fromGeosPolygon( const GEOSGeometry *geos )
{
  if ( GEOSGeomTypeId_r( geosinit.ctxt, geos ) != GEOS_POLYGON )
  {
    return nullptr;
  }

  int nCoordDims = GEOSGeom_getCoordinateDimension_r( geosinit.ctxt, geos );
  int nDims = GEOSGeom_getDimensions_r( geosinit.ctxt, geos );
  bool hasZ = ( nCoordDims == 3 );
  bool hasM = ( ( nDims - nCoordDims ) == 1 );

  QgsPolygonV2 *polygon = new QgsPolygonV2();

  const GEOSGeometry *ring = GEOSGetExteriorRing_r( geosinit.ctxt, geos );
  if ( ring )
  {
    polygon->setExteriorRing( sequenceToLinestring( ring, hasZ, hasM ) );
  }

  QList<QgsCurve *> interiorRings;
  for ( int i = 0; i < GEOSGetNumInteriorRings_r( geosinit.ctxt, geos ); ++i )
  {
    ring = GEOSGetInteriorRingN_r( geosinit.ctxt, geos, i );
    if ( ring )
    {
      interiorRings.push_back( sequenceToLinestring( ring, hasZ, hasM ) );
    }
  }
  polygon->setInteriorRings( interiorRings );

  return polygon;
}

QgsLineString *QgsGeos::sequenceToLinestring( const GEOSGeometry *geos, bool hasZ, bool hasM )
{
  const GEOSCoordSequence *cs = GEOSGeom_getCoordSeq_r( geosinit.ctxt, geos );
  unsigned int nPoints;
  GEOSCoordSeq_getSize_r( geosinit.ctxt, cs, &nPoints );
  QVector< double > xOut;
  xOut.reserve( nPoints );
  QVector< double > yOut;
  yOut.reserve( nPoints );
  QVector< double > zOut;
  if ( hasZ )
    zOut.reserve( nPoints );
  QVector< double > mOut;
  if ( hasM )
    mOut.reserve( nPoints );
  double x = 0;
  double y = 0;
  double z = 0;
  double m = 0;
  for ( unsigned int i = 0; i < nPoints; ++i )
  {
    GEOSCoordSeq_getX_r( geosinit.ctxt, cs, i, &x );
    xOut << x;
    GEOSCoordSeq_getY_r( geosinit.ctxt, cs, i, &y );
    yOut << y;
    if ( hasZ )
    {
      GEOSCoordSeq_getZ_r( geosinit.ctxt, cs, i, &z );
      zOut << z;
    }
    if ( hasM )
    {
      GEOSCoordSeq_getOrdinate_r( geosinit.ctxt, cs, i, 3, &m );
      mOut << m;
    }
  }
  QgsLineString *line = new QgsLineString( xOut, yOut, zOut, mOut );
  return line;
}

int QgsGeos::numberOfGeometries( GEOSGeometry *g )
{
  if ( !g )
    return 0;

  int geometryType = GEOSGeomTypeId_r( geosinit.ctxt, g );
  if ( geometryType == GEOS_POINT || geometryType == GEOS_LINESTRING || geometryType == GEOS_LINEARRING
       || geometryType == GEOS_POLYGON )
    return 1;

  //calling GEOSGetNumGeometries is save for multi types and collections also in geos2
  return GEOSGetNumGeometries_r( geosinit.ctxt, g );
}

QgsPoint QgsGeos::coordSeqPoint( const GEOSCoordSequence *cs, int i, bool hasZ, bool hasM )
{
  if ( !cs )
  {
    return QgsPoint();
  }

  double x, y;
  double z = 0;
  double m = 0;
  GEOSCoordSeq_getX_r( geosinit.ctxt, cs, i, &x );
  GEOSCoordSeq_getY_r( geosinit.ctxt, cs, i, &y );
  if ( hasZ )
  {
    GEOSCoordSeq_getZ_r( geosinit.ctxt, cs, i, &z );
  }
  if ( hasM )
  {
    GEOSCoordSeq_getOrdinate_r( geosinit.ctxt, cs, i, 3, &m );
  }

  QgsWkbTypes::Type t = QgsWkbTypes::Point;
  if ( hasZ && hasM )
  {
    t = QgsWkbTypes::PointZM;
  }
  else if ( hasZ )
  {
    t = QgsWkbTypes::PointZ;
  }
  else if ( hasM )
  {
    t = QgsWkbTypes::PointM;
  }
  return QgsPoint( t, x, y, z, m );
}

GEOSGeometry *QgsGeos::asGeos( const QgsAbstractGeometry *geom, double precision )
{
  if ( !geom )
    return nullptr;

  int coordDims = 2;
  if ( geom->is3D() )
  {
    ++coordDims;
  }
  if ( geom->isMeasure() )
  {
    ++coordDims;
  }

  if ( QgsWkbTypes::isMultiType( geom->wkbType() )  || QgsWkbTypes::flatType( geom->wkbType() ) == QgsWkbTypes::GeometryCollection )
  {
    int geosType = GEOS_GEOMETRYCOLLECTION;

    if ( QgsWkbTypes::flatType( geom->wkbType() ) != QgsWkbTypes::GeometryCollection )
    {
      switch ( QgsWkbTypes::geometryType( geom->wkbType() ) )
      {
        case QgsWkbTypes::PointGeometry:
          geosType = GEOS_MULTIPOINT;
          break;

        case QgsWkbTypes::LineGeometry:
          geosType = GEOS_MULTILINESTRING;
          break;

        case QgsWkbTypes::PolygonGeometry:
          geosType = GEOS_MULTIPOLYGON;
          break;

        case QgsWkbTypes::UnknownGeometry:
        case QgsWkbTypes::NullGeometry:
          return nullptr;
          break;
      }
    }


    const QgsGeometryCollection *c = qgsgeometry_cast<const QgsGeometryCollection *>( geom );

    if ( !c )
      return nullptr;

    QVector< GEOSGeometry * > geomVector( c->numGeometries() );
    for ( int i = 0; i < c->numGeometries(); ++i )
    {
      geomVector[i] = asGeos( c->geometryN( i ), precision );
    }
    return createGeosCollection( geosType, geomVector );
  }
  else
  {
    switch ( QgsWkbTypes::geometryType( geom->wkbType() ) )
    {
      case QgsWkbTypes::PointGeometry:
        return createGeosPoint( static_cast<const QgsPoint *>( geom ), coordDims, precision );
        break;

      case QgsWkbTypes::LineGeometry:
        return createGeosLinestring( static_cast<const QgsLineString *>( geom ), precision );
        break;

      case QgsWkbTypes::PolygonGeometry:
        return createGeosPolygon( static_cast<const QgsPolygonV2 *>( geom ), precision );
        break;

      case QgsWkbTypes::UnknownGeometry:
      case QgsWkbTypes::NullGeometry:
        return nullptr;
        break;
    }
  }
  return nullptr;
}

QgsAbstractGeometry *QgsGeos::overlay( const QgsAbstractGeometry *geom, Overlay op, QString *errorMsg ) const
{
  if ( !mGeos || !geom )
  {
    return nullptr;
  }

  GEOSGeomScopedPtr geosGeom( asGeos( geom, mPrecision ) );
  if ( !geosGeom )
  {
    return nullptr;
  }

  try
  {
    GEOSGeomScopedPtr opGeom;
    switch ( op )
    {
      case INTERSECTION:
        opGeom.reset( GEOSIntersection_r( geosinit.ctxt, mGeos, geosGeom.get() ) );
        break;
      case DIFFERENCE:
        opGeom.reset( GEOSDifference_r( geosinit.ctxt, mGeos, geosGeom.get() ) );
        break;
      case UNION:
      {
        GEOSGeometry *unionGeometry = GEOSUnion_r( geosinit.ctxt, mGeos, geosGeom.get() );

        if ( unionGeometry && GEOSGeomTypeId_r( geosinit.ctxt, unionGeometry ) == GEOS_MULTILINESTRING )
        {
          GEOSGeometry *mergedLines = GEOSLineMerge_r( geosinit.ctxt, unionGeometry );
          if ( mergedLines )
          {
            GEOSGeom_destroy_r( geosinit.ctxt, unionGeometry );
            unionGeometry = mergedLines;
          }
        }

        opGeom.reset( unionGeometry );
      }
      break;
      case SYMDIFFERENCE:
        opGeom.reset( GEOSSymDifference_r( geosinit.ctxt, mGeos, geosGeom.get() ) );
        break;
      default:    //unknown op
        return nullptr;
    }
    QgsAbstractGeometry *opResult = fromGeos( opGeom.get() );
    return opResult;
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return nullptr;
  }
}

bool QgsGeos::relation( const QgsAbstractGeometry *geom, Relation r, QString *errorMsg ) const
{
  if ( !mGeos || !geom )
  {
    return false;
  }

  GEOSGeomScopedPtr geosGeom( asGeos( geom, mPrecision ) );
  if ( !geosGeom )
  {
    return false;
  }

  bool result = false;
  try
  {
    if ( mGeosPrepared ) //use faster version with prepared geometry
    {
      switch ( r )
      {
        case INTERSECTS:
          result = ( GEOSPreparedIntersects_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case TOUCHES:
          result = ( GEOSPreparedTouches_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case CROSSES:
          result = ( GEOSPreparedCrosses_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case WITHIN:
          result = ( GEOSPreparedWithin_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case CONTAINS:
          result = ( GEOSPreparedContains_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case DISJOINT:
          result = ( GEOSPreparedDisjoint_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        case OVERLAPS:
          result = ( GEOSPreparedOverlaps_r( geosinit.ctxt, mGeosPrepared, geosGeom.get() ) == 1 );
          break;
        default:
          return false;
      }
      return result;
    }

    switch ( r )
    {
      case INTERSECTS:
        result = ( GEOSIntersects_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case TOUCHES:
        result = ( GEOSTouches_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case CROSSES:
        result = ( GEOSCrosses_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case WITHIN:
        result = ( GEOSWithin_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case CONTAINS:
        result = ( GEOSContains_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case DISJOINT:
        result = ( GEOSDisjoint_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      case OVERLAPS:
        result = ( GEOSOverlaps_r( geosinit.ctxt, mGeos, geosGeom.get() ) == 1 );
        break;
      default:
        return false;
    }
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return false;
  }

  return result;
}

QgsAbstractGeometry *QgsGeos::buffer( double distance, int segments, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSBuffer_r( geosinit.ctxt, mGeos, distance, segments ) );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsAbstractGeometry *QgsGeos::buffer( double distance, int segments, int endCapStyle, int joinStyle, double miterLimit, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSBufferWithStyle_r( geosinit.ctxt, mGeos, distance, segments, endCapStyle, joinStyle, miterLimit ) );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsAbstractGeometry *QgsGeos::simplify( double tolerance, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }
  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSTopologyPreserveSimplify_r( geosinit.ctxt, mGeos, tolerance ) );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsAbstractGeometry *QgsGeos::interpolate( double distance, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }
  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSInterpolate_r( geosinit.ctxt, mGeos, distance ) );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsPoint *QgsGeos::centroid( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  GEOSGeomScopedPtr geos;
  double x;
  double y;

  try
  {
    geos.reset( GEOSGetCentroid_r( geosinit.ctxt,  mGeos ) );

    if ( !geos )
      return nullptr;

    GEOSGeomGetX_r( geosinit.ctxt, geos.get(), &x );
    GEOSGeomGetY_r( geosinit.ctxt, geos.get(), &y );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );

  return new QgsPoint( x, y );
}

QgsAbstractGeometry *QgsGeos::envelope( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }
  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSEnvelope_r( geosinit.ctxt, mGeos ) );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsPoint *QgsGeos::pointOnSurface( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  double x;
  double y;

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSPointOnSurface_r( geosinit.ctxt, mGeos ) );

    if ( !geos || GEOSisEmpty_r( geosinit.ctxt, geos.get() ) != 0 )
    {
      return nullptr;
    }

    GEOSGeomGetX_r( geosinit.ctxt, geos.get(), &x );
    GEOSGeomGetY_r( geosinit.ctxt, geos.get(), &y );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );

  return new QgsPoint( x, y );
}

QgsAbstractGeometry *QgsGeos::convexHull( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  try
  {
    GEOSGeometry *cHull = GEOSConvexHull_r( geosinit.ctxt, mGeos );
    QgsAbstractGeometry *cHullGeom = fromGeos( cHull );
    GEOSGeom_destroy_r( geosinit.ctxt, cHull );
    return cHullGeom;
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
}

bool QgsGeos::isValid( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return false;
  }

  try
  {
    return GEOSisValid_r( geosinit.ctxt, mGeos );
  }
  CATCH_GEOS_WITH_ERRMSG( false );
}

bool QgsGeos::isEqual( const QgsAbstractGeometry *geom, QString *errorMsg ) const
{
  if ( !mGeos || !geom )
  {
    return false;
  }

  try
  {
    GEOSGeomScopedPtr geosGeom( asGeos( geom, mPrecision ) );
    if ( !geosGeom )
    {
      return false;
    }
    bool equal = GEOSEquals_r( geosinit.ctxt, mGeos, geosGeom.get() );
    return equal;
  }
  CATCH_GEOS_WITH_ERRMSG( false );
}

bool QgsGeos::isEmpty( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return false;
  }

  try
  {
    return GEOSisEmpty_r( geosinit.ctxt, mGeos );
  }
  CATCH_GEOS_WITH_ERRMSG( false );
}

bool QgsGeos::isSimple( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return false;
  }

  try
  {
    return GEOSisSimple_r( geosinit.ctxt, mGeos );
  }
  CATCH_GEOS_WITH_ERRMSG( false );
}

GEOSCoordSequence *QgsGeos::createCoordinateSequence( const QgsCurve *curve, double precision, bool forceClose )
{
  std::unique_ptr< QgsLineString > segmentized;
  const QgsLineString *line = qgsgeometry_cast<const QgsLineString *>( curve );

  if ( !line )
  {
    segmentized.reset( curve->curveToLine() );
    line = segmentized.get();
  }

  if ( !line )
  {
    return nullptr;
  }

  bool hasZ = line->is3D();
  bool hasM = false; //line->isMeasure(); //disabled until geos supports m-coordinates
  int coordDims = 2;
  if ( hasZ )
  {
    ++coordDims;
  }
  if ( hasM )
  {
    ++coordDims;
  }

  int numPoints = line->numPoints();

  int numOutPoints = numPoints;
  if ( forceClose && ( line->pointN( 0 ) != line->pointN( numPoints - 1 ) ) )
  {
    ++numOutPoints;
  }

  GEOSCoordSequence *coordSeq = nullptr;
  try
  {
    coordSeq = GEOSCoordSeq_create_r( geosinit.ctxt, numOutPoints, coordDims );
    if ( !coordSeq )
    {
      QgsMessageLog::logMessage( QObject::tr( "Could not create coordinate sequence for %1 points in %2 dimensions" ).arg( numPoints ).arg( coordDims ), QObject::tr( "GEOS" ) );
      return nullptr;
    }
    if ( precision > 0. )
    {
      for ( int i = 0; i < numOutPoints; ++i )
      {
        GEOSCoordSeq_setX_r( geosinit.ctxt, coordSeq, i, std::round( line->xAt( i % numPoints ) / precision ) * precision );
        GEOSCoordSeq_setY_r( geosinit.ctxt, coordSeq, i, std::round( line->yAt( i % numPoints ) / precision ) * precision );
        if ( hasZ )
        {
          GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, i, 2, std::round( line->zAt( i % numPoints ) / precision ) * precision );
        }
        if ( hasM )
        {
          GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, i, 3, line->mAt( i % numPoints ) );
        }
      }
    }
    else
    {
      for ( int i = 0; i < numOutPoints; ++i )
      {
        GEOSCoordSeq_setX_r( geosinit.ctxt, coordSeq, i, line->xAt( i % numPoints ) );
        GEOSCoordSeq_setY_r( geosinit.ctxt, coordSeq, i, line->yAt( i % numPoints ) );
        if ( hasZ )
        {
          GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, i, 2, line->zAt( i % numPoints ) );
        }
        if ( hasM )
        {
          GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, i, 3, line->mAt( i % numPoints ) );
        }
      }
    }
  }
  CATCH_GEOS( nullptr )

  return coordSeq;
}

GEOSGeometry *QgsGeos::createGeosPoint( const QgsAbstractGeometry *point, int coordDims, double precision )
{
  const QgsPoint *pt = qgsgeometry_cast<const QgsPoint *>( point );
  if ( !pt )
    return nullptr;

  return createGeosPointXY( pt->x(), pt->y(), pt->is3D(), pt->z(), pt->isMeasure(), pt->m(), coordDims, precision );
}

GEOSGeometry *QgsGeos::createGeosPointXY( double x, double y, bool hasZ, double z, bool hasM, double m, int coordDims, double precision )
{
  Q_UNUSED( hasM );
  Q_UNUSED( m );

  GEOSGeometry *geosPoint = nullptr;

  try
  {
    GEOSCoordSequence *coordSeq = GEOSCoordSeq_create_r( geosinit.ctxt, 1, coordDims );
    if ( !coordSeq )
    {
      QgsMessageLog::logMessage( QObject::tr( "Could not create coordinate sequence for point with %1 dimensions" ).arg( coordDims ), QObject::tr( "GEOS" ) );
      return nullptr;
    }
    if ( precision > 0. )
    {
      GEOSCoordSeq_setX_r( geosinit.ctxt, coordSeq, 0, std::round( x / precision ) * precision );
      GEOSCoordSeq_setY_r( geosinit.ctxt, coordSeq, 0, std::round( y / precision ) * precision );
      if ( hasZ )
      {
        GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, 0, 2, std::round( z / precision ) * precision );
      }
    }
    else
    {
      GEOSCoordSeq_setX_r( geosinit.ctxt, coordSeq, 0, x );
      GEOSCoordSeq_setY_r( geosinit.ctxt, coordSeq, 0, y );
      if ( hasZ )
      {
        GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, 0, 2, z );
      }
    }
#if 0 //disabled until geos supports m-coordinates
    if ( hasM )
    {
      GEOSCoordSeq_setOrdinate_r( geosinit.ctxt, coordSeq, 0, 3, m );
    }
#endif
    geosPoint = GEOSGeom_createPoint_r( geosinit.ctxt, coordSeq );
  }
  CATCH_GEOS( nullptr )
  return geosPoint;
}

GEOSGeometry *QgsGeos::createGeosLinestring( const QgsAbstractGeometry *curve, double precision )
{
  const QgsCurve *c = qgsgeometry_cast<const QgsCurve *>( curve );
  if ( !c )
    return nullptr;

  GEOSCoordSequence *coordSeq = createCoordinateSequence( c, precision );
  if ( !coordSeq )
    return nullptr;

  GEOSGeometry *geosGeom = nullptr;
  try
  {
    geosGeom = GEOSGeom_createLineString_r( geosinit.ctxt, coordSeq );
  }
  CATCH_GEOS( nullptr )
  return geosGeom;
}

GEOSGeometry *QgsGeos::createGeosPolygon( const QgsAbstractGeometry *poly, double precision )
{
  const QgsCurvePolygon *polygon = qgsgeometry_cast<const QgsCurvePolygon *>( poly );
  if ( !polygon )
    return nullptr;

  const QgsCurve *exteriorRing = polygon->exteriorRing();
  if ( !exteriorRing )
  {
    return nullptr;
  }

  GEOSGeometry *geosPolygon = nullptr;
  try
  {
    GEOSGeometry *exteriorRingGeos = GEOSGeom_createLinearRing_r( geosinit.ctxt, createCoordinateSequence( exteriorRing, precision, true ) );


    int nHoles = polygon->numInteriorRings();
    GEOSGeometry **holes = nullptr;
    if ( nHoles > 0 )
    {
      holes = new GEOSGeometry*[ nHoles ];
    }

    for ( int i = 0; i < nHoles; ++i )
    {
      const QgsCurve *interiorRing = polygon->interiorRing( i );
      holes[i] = GEOSGeom_createLinearRing_r( geosinit.ctxt, createCoordinateSequence( interiorRing, precision, true ) );
    }
    geosPolygon = GEOSGeom_createPolygon_r( geosinit.ctxt, exteriorRingGeos, holes, nHoles );
    delete[] holes;
  }
  CATCH_GEOS( nullptr )

  return geosPolygon;
}

QgsAbstractGeometry *QgsGeos::offsetCurve( double distance, int segments, int joinStyle, double miterLimit, QString *errorMsg ) const
{
  if ( !mGeos )
    return nullptr;

  GEOSGeometry *offset = nullptr;
  try
  {
    offset = GEOSOffsetCurve_r( geosinit.ctxt, mGeos, distance, segments, joinStyle, miterLimit );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr )
  QgsAbstractGeometry *offsetGeom = fromGeos( offset );
  GEOSGeom_destroy_r( geosinit.ctxt, offset );
  return offsetGeom;
}

QgsAbstractGeometry *QgsGeos::singleSidedBuffer( double distance, int segments, int side, int joinStyle, double miterLimit, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return nullptr;
  }

  GEOSGeomScopedPtr geos;
  try
  {
    GEOSBufferParams *bp  = GEOSBufferParams_create_r( geosinit.ctxt );
    GEOSBufferParams_setSingleSided_r( geosinit.ctxt, bp, 1 );
    GEOSBufferParams_setQuadrantSegments_r( geosinit.ctxt, bp, segments );
    GEOSBufferParams_setJoinStyle_r( geosinit.ctxt, bp, joinStyle );
    GEOSBufferParams_setMitreLimit_r( geosinit.ctxt, bp, miterLimit );  //#spellok

    if ( side == 1 )
    {
      distance = -distance;
    }
    geos.reset( GEOSBufferWithParams_r( geosinit.ctxt, mGeos, bp, distance ) );
    GEOSBufferParams_destroy_r( geosinit.ctxt, bp );
  }
  CATCH_GEOS_WITH_ERRMSG( nullptr );
  return fromGeos( geos.get() );
}

QgsAbstractGeometry *QgsGeos::reshapeGeometry( const QgsLineString &reshapeWithLine, EngineOperationResult *errorCode, QString *errorMsg ) const
{
  if ( !mGeos || mGeometry->dimension() == 0 )
  {
    if ( errorCode ) { *errorCode = InvalidBaseGeometry; }
    return nullptr;
  }

  if ( reshapeWithLine.numPoints() < 2 )
  {
    if ( errorCode ) { *errorCode = InvalidInput; }
    return nullptr;
  }

  GEOSGeometry *reshapeLineGeos = createGeosLinestring( &reshapeWithLine, mPrecision );

  //single or multi?
  int numGeoms = GEOSGetNumGeometries_r( geosinit.ctxt, mGeos );
  if ( numGeoms == -1 )
  {
    if ( errorCode ) { *errorCode = InvalidBaseGeometry; }
    GEOSGeom_destroy_r( geosinit.ctxt, reshapeLineGeos );
    return nullptr;
  }

  bool isMultiGeom = false;
  int geosTypeId = GEOSGeomTypeId_r( geosinit.ctxt, mGeos );
  if ( geosTypeId == GEOS_MULTILINESTRING || geosTypeId == GEOS_MULTIPOLYGON )
    isMultiGeom = true;

  bool isLine = ( mGeometry->dimension() == 1 );

  if ( !isMultiGeom )
  {
    GEOSGeometry *reshapedGeometry = nullptr;
    if ( isLine )
    {
      reshapedGeometry = reshapeLine( mGeos, reshapeLineGeos, mPrecision );
    }
    else
    {
      reshapedGeometry = reshapePolygon( mGeos, reshapeLineGeos, mPrecision );
    }

    if ( errorCode )
      *errorCode = Success;
    QgsAbstractGeometry *reshapeResult = fromGeos( reshapedGeometry );
    GEOSGeom_destroy_r( geosinit.ctxt, reshapedGeometry );
    GEOSGeom_destroy_r( geosinit.ctxt, reshapeLineGeos );
    return reshapeResult;
  }
  else
  {
    try
    {
      //call reshape for each geometry part and replace mGeos with new geometry if reshape took place
      bool reshapeTookPlace = false;

      GEOSGeometry *currentReshapeGeometry = nullptr;
      GEOSGeometry **newGeoms = new GEOSGeometry*[numGeoms];

      for ( int i = 0; i < numGeoms; ++i )
      {
        if ( isLine )
          currentReshapeGeometry = reshapeLine( GEOSGetGeometryN_r( geosinit.ctxt, mGeos, i ), reshapeLineGeos, mPrecision );
        else
          currentReshapeGeometry = reshapePolygon( GEOSGetGeometryN_r( geosinit.ctxt, mGeos, i ), reshapeLineGeos, mPrecision );

        if ( currentReshapeGeometry )
        {
          newGeoms[i] = currentReshapeGeometry;
          reshapeTookPlace = true;
        }
        else
        {
          newGeoms[i] = GEOSGeom_clone_r( geosinit.ctxt, GEOSGetGeometryN_r( geosinit.ctxt, mGeos, i ) );
        }
      }
      GEOSGeom_destroy_r( geosinit.ctxt, reshapeLineGeos );

      GEOSGeometry *newMultiGeom = nullptr;
      if ( isLine )
      {
        newMultiGeom = GEOSGeom_createCollection_r( geosinit.ctxt, GEOS_MULTILINESTRING, newGeoms, numGeoms );
      }
      else //multipolygon
      {
        newMultiGeom = GEOSGeom_createCollection_r( geosinit.ctxt, GEOS_MULTIPOLYGON, newGeoms, numGeoms );
      }

      delete[] newGeoms;
      if ( !newMultiGeom )
      {
        if ( errorCode ) { *errorCode = EngineError; }
        return nullptr;
      }

      if ( reshapeTookPlace )
      {
        if ( errorCode )
          *errorCode = Success;
        QgsAbstractGeometry *reshapedMultiGeom = fromGeos( newMultiGeom );
        GEOSGeom_destroy_r( geosinit.ctxt, newMultiGeom );
        return reshapedMultiGeom;
      }
      else
      {
        GEOSGeom_destroy_r( geosinit.ctxt, newMultiGeom );
        if ( errorCode ) { *errorCode = NothingHappened; }
        return nullptr;
      }
    }
    CATCH_GEOS_WITH_ERRMSG( nullptr )
  }
}

QgsGeometry QgsGeos::mergeLines( QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return QgsGeometry();
  }

  if ( GEOSGeomTypeId_r( geosinit.ctxt, mGeos ) != GEOS_MULTILINESTRING )
    return QgsGeometry();

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSLineMerge_r( geosinit.ctxt, mGeos ) );
  }
  CATCH_GEOS_WITH_ERRMSG( QgsGeometry() );
  return QgsGeometry( fromGeos( geos.get() ) );
}

QgsGeometry QgsGeos::closestPoint( const QgsGeometry &other, QString *errorMsg ) const
{
  if ( !mGeos || other.isNull() )
  {
    return QgsGeometry();
  }

  GEOSGeomScopedPtr otherGeom( asGeos( other.geometry(), mPrecision ) );
  if ( !otherGeom )
  {
    return QgsGeometry();
  }

  double nx = 0.0;
  double ny = 0.0;
  try
  {
    GEOSCoordSequence *nearestCoord = GEOSNearestPoints_r( geosinit.ctxt, mGeos, otherGeom.get() );

    ( void )GEOSCoordSeq_getX_r( geosinit.ctxt, nearestCoord, 0, &nx );
    ( void )GEOSCoordSeq_getY_r( geosinit.ctxt, nearestCoord, 0, &ny );
    GEOSCoordSeq_destroy_r( geosinit.ctxt, nearestCoord );
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return QgsGeometry();
  }

  return QgsGeometry( new QgsPoint( nx, ny ) );
}

QgsGeometry QgsGeos::shortestLine( const QgsGeometry &other, QString *errorMsg ) const
{
  if ( !mGeos || other.isNull() )
  {
    return QgsGeometry();
  }

  GEOSGeomScopedPtr otherGeom( asGeos( other.geometry(), mPrecision ) );
  if ( !otherGeom )
  {
    return QgsGeometry();
  }

  double nx1 = 0.0;
  double ny1 = 0.0;
  double nx2 = 0.0;
  double ny2 = 0.0;
  try
  {
    GEOSCoordSequence *nearestCoord = GEOSNearestPoints_r( geosinit.ctxt, mGeos, otherGeom.get() );

    ( void )GEOSCoordSeq_getX_r( geosinit.ctxt, nearestCoord, 0, &nx1 );
    ( void )GEOSCoordSeq_getY_r( geosinit.ctxt, nearestCoord, 0, &ny1 );
    ( void )GEOSCoordSeq_getX_r( geosinit.ctxt, nearestCoord, 1, &nx2 );
    ( void )GEOSCoordSeq_getY_r( geosinit.ctxt, nearestCoord, 1, &ny2 );

    GEOSCoordSeq_destroy_r( geosinit.ctxt, nearestCoord );
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return QgsGeometry();
  }

  QgsLineString *line = new QgsLineString();
  line->addVertex( QgsPoint( nx1, ny1 ) );
  line->addVertex( QgsPoint( nx2, ny2 ) );
  return QgsGeometry( line );
}

double QgsGeos::lineLocatePoint( const QgsPoint &point, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return -1;
  }

  GEOSGeomScopedPtr otherGeom( asGeos( &point, mPrecision ) );
  if ( !otherGeom )
  {
    return -1;
  }

  double distance = -1;
  try
  {
    distance = GEOSProject_r( geosinit.ctxt, mGeos, otherGeom.get() );
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    return -1;
  }

  return distance;
}

QgsGeometry QgsGeos::polygonize( const QList<QgsAbstractGeometry *> &geometries, QString *errorMsg )
{
  GEOSGeometry **const lineGeosGeometries = new GEOSGeometry*[ geometries.size()];
  int validLines = 0;
  for ( const QgsAbstractGeometry *g : geometries )
  {
    GEOSGeometry *l = asGeos( g );
    if ( l )
    {
      lineGeosGeometries[validLines] = l;
      validLines++;
    }
  }

  try
  {
    GEOSGeomScopedPtr result( GEOSPolygonize_r( geosinit.ctxt, lineGeosGeometries, validLines ) );
    for ( int i = 0; i < validLines; ++i )
    {
      GEOSGeom_destroy_r( geosinit.ctxt, lineGeosGeometries[i] );
    }
    delete[] lineGeosGeometries;
    return QgsGeometry( fromGeos( result.get() ) );
  }
  catch ( GEOSException &e )
  {
    if ( errorMsg )
    {
      *errorMsg = e.what();
    }
    for ( int i = 0; i < validLines; ++i )
    {
      GEOSGeom_destroy_r( geosinit.ctxt, lineGeosGeometries[i] );
    }
    delete[] lineGeosGeometries;
    return QgsGeometry();
  }
}

QgsGeometry QgsGeos::voronoiDiagram( const QgsAbstractGeometry *extent, double tolerance, bool edgesOnly, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return QgsGeometry();
  }

  GEOSGeometry *extentGeos = nullptr;
  GEOSGeomScopedPtr extentGeosGeom( nullptr );
  if ( extent )
  {
    extentGeosGeom.reset( asGeos( extent, mPrecision ) );
    if ( !extentGeosGeom )
    {
      return QgsGeometry();
    }
    extentGeos = extentGeosGeom.get();
  }

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSVoronoiDiagram_r( geosinit.ctxt, mGeos, extentGeos, tolerance, edgesOnly ) );

    if ( !geos || GEOSisEmpty_r( geosinit.ctxt, geos.get() ) != 0 )
    {
      return QgsGeometry();
    }

    return QgsGeometry( fromGeos( geos.get() ) );
  }
  CATCH_GEOS_WITH_ERRMSG( QgsGeometry() );
}

QgsGeometry QgsGeos::delaunayTriangulation( double tolerance, bool edgesOnly, QString *errorMsg ) const
{
  if ( !mGeos )
  {
    return QgsGeometry();
  }

  GEOSGeomScopedPtr geos;
  try
  {
    geos.reset( GEOSDelaunayTriangulation_r( geosinit.ctxt, mGeos, tolerance, edgesOnly ) );

    if ( !geos || GEOSisEmpty_r( geosinit.ctxt, geos.get() ) != 0 )
    {
      return QgsGeometry();
    }

    return QgsGeometry( fromGeos( geos.get() ) );
  }
  CATCH_GEOS_WITH_ERRMSG( QgsGeometry() );
}


//! Extract coordinates of linestring's endpoints. Returns false on error.
static bool _linestringEndpoints( const GEOSGeometry *linestring, double &x1, double &y1, double &x2, double &y2 )
{
  const GEOSCoordSequence *coordSeq = GEOSGeom_getCoordSeq_r( geosinit.ctxt, linestring );
  if ( !coordSeq )
    return false;

  unsigned int coordSeqSize;
  if ( GEOSCoordSeq_getSize_r( geosinit.ctxt, coordSeq, &coordSeqSize ) == 0 )
    return false;

  if ( coordSeqSize < 2 )
    return false;

  GEOSCoordSeq_getX_r( geosinit.ctxt, coordSeq, 0, &x1 );
  GEOSCoordSeq_getY_r( geosinit.ctxt, coordSeq, 0, &y1 );
  GEOSCoordSeq_getX_r( geosinit.ctxt, coordSeq, coordSeqSize - 1, &x2 );
  GEOSCoordSeq_getY_r( geosinit.ctxt, coordSeq, coordSeqSize - 1, &y2 );
  return true;
}


//! Merge two linestrings if they meet at the given intersection point, return new geometry or null on error.
static GEOSGeometry *_mergeLinestrings( const GEOSGeometry *line1, const GEOSGeometry *line2, const QgsPointXY &intersectionPoint )
{
  double x1, y1, x2, y2;
  if ( !_linestringEndpoints( line1, x1, y1, x2, y2 ) )
    return nullptr;

  double rx1, ry1, rx2, ry2;
  if ( !_linestringEndpoints( line2, rx1, ry1, rx2, ry2 ) )
    return nullptr;

  bool intersectionAtOrigLineEndpoint =
    ( intersectionPoint.x() == x1 && intersectionPoint.y() == y1 ) ||
    ( intersectionPoint.x() == x2 && intersectionPoint.y() == y2 );
  bool intersectionAtReshapeLineEndpoint =
    ( intersectionPoint.x() == rx1 && intersectionPoint.y() == ry1 ) ||
    ( intersectionPoint.x() == rx2 && intersectionPoint.y() == ry2 );

  // the intersection must be at the begin/end of both lines
  if ( intersectionAtOrigLineEndpoint && intersectionAtReshapeLineEndpoint )
  {
    GEOSGeometry *g1 = GEOSGeom_clone_r( geosinit.ctxt, line1 );
    GEOSGeometry *g2 = GEOSGeom_clone_r( geosinit.ctxt, line2 );
    GEOSGeometry *geoms[2] = { g1, g2 };
    GEOSGeometry *multiGeom = GEOSGeom_createCollection_r( geosinit.ctxt, GEOS_MULTILINESTRING, geoms, 2 );
    GEOSGeometry *res = GEOSLineMerge_r( geosinit.ctxt, multiGeom );
    GEOSGeom_destroy_r( geosinit.ctxt, multiGeom );
    return res;
  }
  else
    return nullptr;
}


GEOSGeometry *QgsGeos::reshapeLine( const GEOSGeometry *line, const GEOSGeometry *reshapeLineGeos, double precision )
{
  if ( !line || !reshapeLineGeos )
    return nullptr;

  bool atLeastTwoIntersections = false;
  bool oneIntersection = false;
  QgsPointXY oneIntersectionPoint;

  try
  {
    //make sure there are at least two intersection between line and reshape geometry
    GEOSGeometry *intersectGeom = GEOSIntersection_r( geosinit.ctxt, line, reshapeLineGeos );
    if ( intersectGeom )
    {
      atLeastTwoIntersections = ( GEOSGeomTypeId_r( geosinit.ctxt, intersectGeom ) == GEOS_MULTIPOINT
                                  && GEOSGetNumGeometries_r( geosinit.ctxt, intersectGeom ) > 1 );
      // one point is enough when extending line at its endpoint
      if ( GEOSGeomTypeId_r( geosinit.ctxt, intersectGeom ) == GEOS_POINT )
      {
        const GEOSCoordSequence *intersectionCoordSeq = GEOSGeom_getCoordSeq_r( geosinit.ctxt, intersectGeom );
        double xi, yi;
        GEOSCoordSeq_getX_r( geosinit.ctxt, intersectionCoordSeq, 0, &xi );
        GEOSCoordSeq_getY_r( geosinit.ctxt, intersectionCoordSeq, 0, &yi );
        oneIntersection = true;
        oneIntersectionPoint = QgsPointXY( xi, yi );
      }
      GEOSGeom_destroy_r( geosinit.ctxt, intersectGeom );
    }
  }
  catch ( GEOSException &e )
  {
    QgsMessageLog::logMessage( QObject::tr( "Exception: %1" ).arg( e.what() ), QObject::tr( "GEOS" ) );
    atLeastTwoIntersections = false;
  }

  // special case when extending line at its endpoint
  if ( oneIntersection )
    return _mergeLinestrings( line, reshapeLineGeos, oneIntersectionPoint );

  if ( !atLeastTwoIntersections )
    return nullptr;

  //begin and end point of original line
  double x1, y1, x2, y2;
  if ( !_linestringEndpoints( line, x1, y1, x2, y2 ) )
    return nullptr;

  GEOSGeometry *beginLineVertex = createGeosPointXY( x1, y1, false, 0, false, 0, 2, precision );
  GEOSGeometry *endLineVertex = createGeosPointXY( x2, y2, false, 0, false, 0, 2, precision );

  bool isRing = false;
  if ( GEOSGeomTypeId_r( geosinit.ctxt, line ) == GEOS_LINEARRING
       || GEOSEquals_r( geosinit.ctxt, beginLineVertex, endLineVertex ) == 1 )
    isRing = true;

  //node line and reshape line
  GEOSGeometry *nodedGeometry = nodeGeometries( reshapeLineGeos, line );
  if ( !nodedGeometry )
  {
    GEOSGeom_destroy_r( geosinit.ctxt, beginLineVertex );
    GEOSGeom_destroy_r( geosinit.ctxt, endLineVertex );
    return nullptr;
  }

  //and merge them together
  GEOSGeometry *mergedLines = GEOSLineMerge_r( geosinit.ctxt, nodedGeometry );
  GEOSGeom_destroy_r( geosinit.ctxt, nodedGeometry );
  if ( !mergedLines )
  {
    GEOSGeom_destroy_r( geosinit.ctxt, beginLineVertex );
    GEOSGeom_destroy_r( geosinit.ctxt, endLineVertex );
    return nullptr;
  }

  int numMergedLines = GEOSGetNumGeometries_r( geosinit.ctxt, mergedLines );
  if ( numMergedLines < 2 ) //some special cases. Normally it is >2
  {
    GEOSGeom_destroy_r( geosinit.ctxt, beginLineVertex );
    GEOSGeom_destroy_r( geosinit.ctxt, endLineVertex );
    if ( numMergedLines == 1 ) //reshape line is from begin to endpoint. So we keep the reshapeline
      return GEOSGeom_clone_r( geosinit.ctxt, reshapeLineGeos );
    else
      return nullptr;
  }

  QList<GEOSGeometry *> resultLineParts; //collection with the line segments that will be contained in result
  QList<GEOSGeometry *> probableParts; //parts where we can decide on inclusion only after going through all the candidates

  for ( int i = 0; i < numMergedLines; ++i )
  {
    const GEOSGeometry *currentGeom = nullptr;

    currentGeom = GEOSGetGeometryN_r( geosinit.ctxt, mergedLines, i );
    const GEOSCoordSequence *currentCoordSeq = GEOSGeom_getCoordSeq_r( geosinit.ctxt, currentGeom );
    unsigned int currentCoordSeqSize;
    GEOSCoordSeq_getSize_r( geosinit.ctxt, currentCoordSeq, &currentCoordSeqSize );
    if ( currentCoordSeqSize < 2 )
      continue;

    //get the two endpoints of the current line merge result
    double xBegin, xEnd, yBegin, yEnd;
    GEOSCoordSeq_getX_r( geosinit.ctxt, currentCoordSeq, 0, &xBegin );
    GEOSCoordSeq_getY_r( geosinit.ctxt, currentCoordSeq, 0, &yBegin );
    GEOSCoordSeq_getX_r( geosinit.ctxt, currentCoordSeq, currentCoordSeqSize - 1, &xEnd );
    GEOSCoordSeq_getY_r( geosinit.ctxt, currentCoordSeq, currentCoordSeqSize - 1, &yEnd );
    GEOSGeometry *beginCurrentGeomVertex = createGeosPointXY( xBegin, yBegin, false, 0, false, 0, 2, precision );
    GEOSGeometry *endCurrentGeomVertex = createGeosPointXY( xEnd, yEnd, false, 0, false, 0, 2, precision );

    //check how many endpoints of the line merge result are on the (original) line
    int nEndpointsOnOriginalLine = 0;
    if ( pointContainedInLine( beginCurrentGeomVertex, line ) == 1 )
      nEndpointsOnOriginalLine += 1;

    if ( pointContainedInLine( endCurrentGeomVertex, line ) == 1 )
      nEndpointsOnOriginalLine += 1;

    //check how many endpoints equal the endpoints of the original line
    int nEndpointsSameAsOriginalLine = 0;
    if ( GEOSEquals_r( geosinit.ctxt, beginCurrentGeomVertex, beginLineVertex ) == 1
         || GEOSEquals_r( geosinit.ctxt, beginCurrentGeomVertex, endLineVertex ) == 1 )
      nEndpointsSameAsOriginalLine += 1;

    if ( GEOSEquals_r( geosinit.ctxt, endCurrentGeomVertex, beginLineVertex ) == 1
         || GEOSEquals_r( geosinit.ctxt, endCurrentGeomVertex, endLineVertex ) == 1 )
      nEndpointsSameAsOriginalLine += 1;

    //check if the current geometry overlaps the original geometry (GEOSOverlap does not seem to work with linestrings)
    bool currentGeomOverlapsOriginalGeom = false;
    bool currentGeomOverlapsReshapeLine = false;
    if ( lineContainedInLine( currentGeom, line ) == 1 )
      currentGeomOverlapsOriginalGeom = true;

    if ( lineContainedInLine( currentGeom, reshapeLineGeos ) == 1 )
      currentGeomOverlapsReshapeLine = true;

    //logic to decide if this part belongs to the result
    if ( !isRing && nEndpointsSameAsOriginalLine == 1 && nEndpointsOnOriginalLine == 2 && currentGeomOverlapsOriginalGeom )
    {
      resultLineParts.push_back( GEOSGeom_clone_r( geosinit.ctxt, currentGeom ) );
    }
    //for closed rings, we take one segment from the candidate list
    else if ( isRing && nEndpointsOnOriginalLine == 2 && currentGeomOverlapsOriginalGeom )
    {
      probableParts.push_back( GEOSGeom_clone_r( geosinit.ctxt, currentGeom ) );
    }
    else if ( nEndpointsOnOriginalLine == 2 && !currentGeomOverlapsOriginalGeom )
    {
      resultLineParts.push_back( GEOSGeom_clone_r( geosinit.ctxt, currentGeom ) );
    }
    else if ( nEndpointsSameAsOriginalLine == 2 && !currentGeomOverlapsOriginalGeom )
    {
      resultLineParts.push_back( GEOSGeom_clone_r( geosinit.ctxt, currentGeom ) );
    }
    else if ( currentGeomOverlapsOriginalGeom && currentGeomOverlapsReshapeLine )
    {
      resultLineParts.push_back( GEOSGeom_clone_r( geosinit.ctxt, currentGeom ) );
    }

    GEOSGeom_destroy_r( geosinit.ctxt, beginCurrentGeomVertex );
    GEOSGeom_destroy_r( geosinit.ctxt, endCurrentGeomVertex );
  }

  //add the longest segment from the probable list for rings (only used for polygon rings)
  if ( isRing && !probableParts.isEmpty() )
  {
    GEOSGeometry *maxGeom = nullptr; //the longest geometry in the probabla list
    GEOSGeometry *currentGeom = nullptr;
    double maxLength = -std::numeric_limits<double>::max();
    double currentLength = 0;
    for ( int i = 0; i < probableParts.size(); ++i )
    {
      currentGeom = probableParts.at( i );
      GEOSLength_r( geosinit.ctxt, currentGeom, &currentLength );
      if ( currentLength > maxLength )
      {
        maxLength = currentLength;
        GEOSGeom_destroy_r( geosinit.ctxt, maxGeom );
        maxGeom = currentGeom;
      }
      else
      {
        GEOSGeom_destroy_r( geosinit.ctxt, currentGeom );
      }
    }
    resultLineParts.push_back( maxGeom );
  }

  GEOSGeom_destroy_r( geosinit.ctxt, beginLineVertex );
  GEOSGeom_destroy_r( geosinit.ctxt, endLineVertex );
  GEOSGeom_destroy_r( geosinit.ctxt, mergedLines );

  GEOSGeometry *result = nullptr;
  if ( resultLineParts.empty() )
    return nullptr;

  if ( resultLineParts.size() == 1 ) //the whole result was reshaped
  {
    result = resultLineParts[0];
  }
  else //>1
  {
    GEOSGeometry **lineArray = new GEOSGeometry*[resultLineParts.size()];
    for ( int i = 0; i < resultLineParts.size(); ++i )
    {
      lineArray[i] = resultLineParts[i];
    }

    //create multiline from resultLineParts
    GEOSGeometry *multiLineGeom = GEOSGeom_createCollection_r( geosinit.ctxt, GEOS_MULTILINESTRING, lineArray, resultLineParts.size() );
    delete [] lineArray;

    //then do a linemerge with the newly combined partstrings
    result = GEOSLineMerge_r( geosinit.ctxt, multiLineGeom );
    GEOSGeom_destroy_r( geosinit.ctxt, multiLineGeom );
  }

  //now test if the result is a linestring. Otherwise something went wrong
  if ( GEOSGeomTypeId_r( geosinit.ctxt, result ) != GEOS_LINESTRING )
  {
    GEOSGeom_destroy_r( geosinit.ctxt, result );
    return nullptr;
  }

  return result;
}

GEOSGeometry *QgsGeos::reshapePolygon( const GEOSGeometry *polygon, const GEOSGeometry *reshapeLineGeos, double precision )
{
  //go through outer shell and all inner rings and check if there is exactly one intersection of a ring and the reshape line
  int nIntersections = 0;
  int lastIntersectingRing = -2;
  const GEOSGeometry *lastIntersectingGeom = nullptr;

  int nRings = GEOSGetNumInteriorRings_r( geosinit.ctxt, polygon );
  if ( nRings < 0 )
    return nullptr;

  //does outer ring intersect?
  const GEOSGeometry *outerRing = GEOSGetExteriorRing_r( geosinit.ctxt, polygon );
  if ( GEOSIntersects_r( geosinit.ctxt, outerRing, reshapeLineGeos ) == 1 )
  {
    ++nIntersections;
    lastIntersectingRing = -1;
    lastIntersectingGeom = outerRing;
  }

  //do inner rings intersect?
  const GEOSGeometry **innerRings = new const GEOSGeometry*[nRings];

  try
  {
    for ( int i = 0; i < nRings; ++i )
    {
      innerRings[i] = GEOSGetInteriorRingN_r( geosinit.ctxt, polygon, i );
      if ( GEOSIntersects_r( geosinit.ctxt, innerRings[i], reshapeLineGeos ) == 1 )
      {
        ++nIntersections;
        lastIntersectingRing = i;
        lastIntersectingGeom = innerRings[i];
      }
    }
  }
  catch ( GEOSException &e )
  {
    QgsMessageLog::logMessage( QObject::tr( "Exception: %1" ).arg( e.what() ), QObject::tr( "GEOS" ) );
    nIntersections = 0;
  }

  if ( nIntersections != 1 ) //reshape line is only allowed to intersect one ring
  {
    delete [] innerRings;
    return nullptr;
  }

  //we have one intersecting ring, let's try to reshape it
  GEOSGeometry *reshapeResult = reshapeLine( lastIntersectingGeom, reshapeLineGeos, precision );
  if ( !reshapeResult )
  {
    delete [] innerRings;
    return nullptr;
  }

  //if reshaping took place, we need to reassemble the polygon and its rings
  GEOSGeometry *newRing = nullptr;
  const GEOSCoordSequence *reshapeSequence = GEOSGeom_getCoordSeq_r( geosinit.ctxt, reshapeResult );
  GEOSCoordSequence *newCoordSequence = GEOSCoordSeq_clone_r( geosinit.ctxt, reshapeSequence );

  GEOSGeom_destroy_r( geosinit.ctxt, reshapeResult );

  newRing = GEOSGeom_createLinearRing_r( geosinit.ctxt, newCoordSequence );
  if ( !newRing )
  {
    delete [] innerRings;
    return nullptr;
  }

  GEOSGeometry *newOuterRing = nullptr;
  if ( lastIntersectingRing == -1 )
    newOuterRing = newRing;
  else
    newOuterRing = GEOSGeom_clone_r( geosinit.ctxt, outerRing );

  //check if all the rings are still inside the outer boundary
  QList<GEOSGeometry *> ringList;
  if ( nRings > 0 )
  {
    GEOSGeometry *outerRingPoly = GEOSGeom_createPolygon_r( geosinit.ctxt, GEOSGeom_clone_r( geosinit.ctxt, newOuterRing ), nullptr, 0 );
    if ( outerRingPoly )
    {
      GEOSGeometry *currentRing = nullptr;
      for ( int i = 0; i < nRings; ++i )
      {
        if ( lastIntersectingRing == i )
          currentRing = newRing;
        else
          currentRing = GEOSGeom_clone_r( geosinit.ctxt, innerRings[i] );

        //possibly a ring is no longer contained in the result polygon after reshape
        if ( GEOSContains_r( geosinit.ctxt, outerRingPoly, currentRing ) == 1 )
          ringList.push_back( currentRing );
        else
          GEOSGeom_destroy_r( geosinit.ctxt, currentRing );
      }
    }
    GEOSGeom_destroy_r( geosinit.ctxt, outerRingPoly );
  }

  GEOSGeometry **newInnerRings = new GEOSGeometry*[ringList.size()];
  for ( int i = 0; i < ringList.size(); ++i )
    newInnerRings[i] = ringList.at( i );

  delete [] innerRings;

  GEOSGeometry *reshapedPolygon = GEOSGeom_createPolygon_r( geosinit.ctxt, newOuterRing, newInnerRings, ringList.size() );
  delete[] newInnerRings;

  return reshapedPolygon;
}

int QgsGeos::lineContainedInLine( const GEOSGeometry *line1, const GEOSGeometry *line2 )
{
  if ( !line1 || !line2 )
  {
    return -1;
  }

  double bufferDistance = std::pow( 10.0L, geomDigits( line2 ) - 11 );

  GEOSGeometry *bufferGeom = GEOSBuffer_r( geosinit.ctxt, line2, bufferDistance, DEFAULT_QUADRANT_SEGMENTS );
  if ( !bufferGeom )
    return -2;

  GEOSGeometry *intersectionGeom = GEOSIntersection_r( geosinit.ctxt, bufferGeom, line1 );

  //compare ratio between line1Length and intersectGeomLength (usually close to 1 if line1 is contained in line2)
  double intersectGeomLength;
  double line1Length;

  GEOSLength_r( geosinit.ctxt, intersectionGeom, &intersectGeomLength );
  GEOSLength_r( geosinit.ctxt, line1, &line1Length );

  GEOSGeom_destroy_r( geosinit.ctxt, bufferGeom );
  GEOSGeom_destroy_r( geosinit.ctxt, intersectionGeom );

  double intersectRatio = line1Length / intersectGeomLength;
  if ( intersectRatio > 0.9 && intersectRatio < 1.1 )
    return 1;

  return 0;
}

int QgsGeos::pointContainedInLine( const GEOSGeometry *point, const GEOSGeometry *line )
{
  if ( !point || !line )
    return -1;

  double bufferDistance = std::pow( 10.0L, geomDigits( line ) - 11 );

  GEOSGeometry *lineBuffer = GEOSBuffer_r( geosinit.ctxt, line, bufferDistance, 8 );
  if ( !lineBuffer )
    return -2;

  bool contained = false;
  if ( GEOSContains_r( geosinit.ctxt, lineBuffer, point ) == 1 )
    contained = true;

  GEOSGeom_destroy_r( geosinit.ctxt, lineBuffer );
  return contained;
}

int QgsGeos::geomDigits( const GEOSGeometry *geom )
{
  GEOSGeomScopedPtr bbox( GEOSEnvelope_r( geosinit.ctxt, geom ) );
  if ( !bbox.get() )
    return -1;

  const GEOSGeometry *bBoxRing = GEOSGetExteriorRing_r( geosinit.ctxt, bbox.get() );
  if ( !bBoxRing )
    return -1;

  const GEOSCoordSequence *bBoxCoordSeq = GEOSGeom_getCoordSeq_r( geosinit.ctxt, bBoxRing );

  if ( !bBoxCoordSeq )
    return -1;

  unsigned int nCoords = 0;
  if ( !GEOSCoordSeq_getSize_r( geosinit.ctxt, bBoxCoordSeq, &nCoords ) )
    return -1;

  int maxDigits = -1;
  for ( unsigned int i = 0; i < nCoords - 1; ++i )
  {
    double t;
    GEOSCoordSeq_getX_r( geosinit.ctxt, bBoxCoordSeq, i, &t );

    int digits;
    digits = std::ceil( std::log10( std::fabs( t ) ) );
    if ( digits > maxDigits )
      maxDigits = digits;

    GEOSCoordSeq_getY_r( geosinit.ctxt, bBoxCoordSeq, i, &t );
    digits = std::ceil( std::log10( std::fabs( t ) ) );
    if ( digits > maxDigits )
      maxDigits = digits;
  }

  return maxDigits;
}

GEOSContextHandle_t QgsGeos::getGEOSHandler()
{
  return geosinit.ctxt;
}
