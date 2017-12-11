/// \file BasicMathPlugin.cpp
/// \author Roger James
/// \date 13th November 2013

#include "BasicMathPlugin.h"

#include "DriverCommon.h"

#include <indicom.h>

#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>

#include <limits>
#include <iostream>
#include <map>

namespace INDI
{
namespace AlignmentSubsystem
{
BasicMathPlugin::BasicMathPlugin()
{
    pActualToApparentTransform   = gsl_matrix_alloc(3, 3);
    pApparentToActualTransform   = gsl_matrix_alloc(3, 3);
    pActualToApparentTransform_2 = gsl_matrix_alloc(3, 3);
    pApparentToActualTransform_2 = gsl_matrix_alloc(3, 3);
}

// Destructor

BasicMathPlugin::~BasicMathPlugin()
{
    gsl_matrix_free(pActualToApparentTransform);
    gsl_matrix_free(pApparentToActualTransform);
    gsl_matrix_free(pActualToApparentTransform_2);
    gsl_matrix_free(pApparentToActualTransform_2);
}

// Public methods

bool BasicMathPlugin::Initialise(InMemoryDatabase *pInMemoryDatabase)
{
    MathPlugin::Initialise(pInMemoryDatabase);
    InMemoryDatabase::AlignmentDatabaseType &SyncPoints = pInMemoryDatabase->GetAlignmentDatabase();

    /// See how many entries there are in the in memory database.
    /// - If just one use a hint to mounts approximate alignment, this can either be ZENITH,
    /// NORTH_CELESTIAL_POLE or SOUTH_CELESTIAL_POLE. The hint is used to make a dummy second
    /// entry. A dummy third entry is computed from the cross product of the first two. A transform
    /// matrix is then computed.
    /// - If two make the dummy third entry and compute a transform matrix.
    /// - If three compute a transform matrix.
    /// - If four or more compute a convex hull, then matrices for each
    /// triangular facet of the hull.
    switch (SyncPoints.size())
    {
        case 0:
            // Not sure whether to return false or true here
            return true;

        case 1:
        {
            AlignmentDatabaseEntry &Entry1 = SyncPoints[0];
            ln_equ_posn RaDec;
            ln_lnlat_posn Position { 0, 0 };

            if (!pInMemoryDatabase->GetDatabaseReferencePosition(Position))
                return false;
            RaDec.dec = Entry1.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec.ra = Entry1.RightAscension * 360.0 / 24.0;
            TelescopeDirectionVector ActualDirectionCosine1;
            TelescopeDirectionVector DummyActualDirectionCosine2;
            TelescopeDirectionVector DummyApparentDirectionCosine2;
            TelescopeDirectionVector DummyActualDirectionCosine3;
            TelescopeDirectionVector DummyApparentDirectionCosine3;

            switch (ApproximateMountAlignment)
            {
                case ZENITH:
		{
		    ln_hrz_posn ActualSyncPoint1;
		    ln_get_hrz_from_equ(&RaDec, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
		    // Now express this coordinate as a normalised direction vector (a.k.a direction cosines)
		    ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
                    DummyActualDirectionCosine2.x = 0.0;
                    DummyActualDirectionCosine2.y = 0.0;
                    DummyActualDirectionCosine2.z = 1.0;
                    DummyApparentDirectionCosine2 = DummyActualDirectionCosine2;
                    break;
		}
                case NORTH_CELESTIAL_POLE:
	        case SOUTH_CELESTIAL_POLE:
                {
		    ln_equ_posn ActualSyncPoint1;
		    double lstdegrees = (ln_get_apparent_sidereal_time(Entry1.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualSyncPoint1.ra = (lstdegrees - RaDec.ra);
		    ActualSyncPoint1.dec = RaDec.dec;
		    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
                    ln_equ_posn DummyRaDec;
		    
		    gsl_matrix *R =          gsl_matrix_alloc(3,3);
		    gsl_vector *pActual1 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent1 = gsl_vector_alloc(3);
		    gsl_vector *pActual2 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent2 = gsl_vector_alloc(3);
                    //DummyRaDec.ra  = 0.0;
                    //DummyRaDec.dec = 90.0;
		    DummyRaDec.ra  = range24(Entry1.RightAscension + 6.0);
                    DummyRaDec.dec = Entry1.Declination;
		    DummyActualDirectionCosine2   = TelescopeDirectionVectorFromLocalHourAngleDeclination(DummyRaDec);
		    // geehalel: computes the rotation matrix from Actual1 to Apparent1
		    gsl_vector_set(pActual1, 0, ActualDirectionCosine1.x);
		    gsl_vector_set(pActual1, 1, ActualDirectionCosine1.y);
		    gsl_vector_set(pActual1, 2, ActualDirectionCosine1.z);
		    gsl_vector_set(pApparent1, 0, Entry1.TelescopeDirection.x);
		    gsl_vector_set(pApparent1, 1, Entry1.TelescopeDirection.y);
		    gsl_vector_set(pApparent1, 2, Entry1.TelescopeDirection.z);
		    RotationMatrixFromVectors(pActual1, pApparent1, R);
		    Dump3x3("Rot",R);
		    // geehale: and applies the rotation to Actual2 (Dummy point above)
		    gsl_vector_set(pActual2, 0, DummyActualDirectionCosine2.x);
		    gsl_vector_set(pActual2, 1, DummyActualDirectionCosine2.y);
		    gsl_vector_set(pActual2, 2, DummyActualDirectionCosine2.z);
		    MatrixVectorMultiply(R, pActual2, pApparent2);
		    DummyApparentDirectionCosine2.x = gsl_vector_get(pApparent2, 0);
		    DummyApparentDirectionCosine2.y = gsl_vector_get(pApparent2, 1);
		    DummyApparentDirectionCosine2.z = gsl_vector_get(pApparent2, 2);
		    // should be normalised (rotation matrix x unit vector) DummyApparentDirectionCosine2.Normalise();
		    gsl_matrix_free(R);
		    gsl_vector_free(pActual1);
		    gsl_vector_free(pApparent1);
		    gsl_vector_free(pActual2);
		    gsl_vector_free(pApparent2);
                    break;
                }
            }
            DummyActualDirectionCosine3 = ActualDirectionCosine1 * DummyActualDirectionCosine2;
            DummyActualDirectionCosine3.Normalise();
            DummyApparentDirectionCosine3 = Entry1.TelescopeDirection * DummyApparentDirectionCosine2;
            DummyApparentDirectionCosine3.Normalise();
            CalculateTransformMatrices(ActualDirectionCosine1, DummyActualDirectionCosine2, DummyActualDirectionCosine3,
                                       Entry1.TelescopeDirection, DummyApparentDirectionCosine2,
                                       DummyApparentDirectionCosine3, pActualToApparentTransform,
                                       pApparentToActualTransform);
            return true;
        }
        case 2:
        {
            // First compute local horizontal coordinates for the two sync points
            AlignmentDatabaseEntry &Entry1 = SyncPoints[0];
            AlignmentDatabaseEntry &Entry2 = SyncPoints[1];
            ln_equ_posn RaDec1;
            ln_equ_posn RaDec2;
	    TelescopeDirectionVector ActualDirectionCosine1;
	    TelescopeDirectionVector ActualDirectionCosine2;
	    
            RaDec1.dec = Entry1.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec1.ra  = Entry1.RightAscension * 360.0 / 24.0;
            RaDec2.dec = Entry2.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec2.ra = Entry2.RightAscension * 360.0 / 24.0;
            ln_lnlat_posn Position { 0, 0 };
            if (!pInMemoryDatabase->GetDatabaseReferencePosition(Position))
                return false;
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		{
		    ln_hrz_posn ActualSyncPoint1;
		    ln_hrz_posn ActualSyncPoint2;
		    ln_get_hrz_from_equ(&RaDec1, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
		    ln_get_hrz_from_equ(&RaDec2, &Position, Entry2.ObservationJulianDate, &ActualSyncPoint2);
		    // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
		    ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
		    ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);
		    // geehalel: leave as before in ZENITH case
		    TelescopeDirectionVector DummyActualDirectionCosine3;
		    TelescopeDirectionVector DummyApparentDirectionCosine3;
		    DummyActualDirectionCosine3 = ActualDirectionCosine1 * ActualDirectionCosine2;
		    DummyActualDirectionCosine3.Normalise();
		    DummyApparentDirectionCosine3 = Entry1.TelescopeDirection * Entry2.TelescopeDirection;
		    DummyApparentDirectionCosine3.Normalise();

		    // The third direction vectors is generated by taking the cross product of the first two
		    CalculateTransformMatrices(ActualDirectionCosine1, ActualDirectionCosine2, DummyActualDirectionCosine3,
					       Entry1.TelescopeDirection, Entry2.TelescopeDirection,
					       DummyApparentDirectionCosine3, pActualToApparentTransform,
					       pApparentToActualTransform);
		    pActualToApparentTransform_2 = pActualToApparentTransform;
		    pApparentToActualTransform_2 = pApparentToActualTransform;
		    break;
		}
	        case NORTH_CELESTIAL_POLE:
	        case SOUTH_CELESTIAL_POLE:
		{
		    ln_equ_posn ActualSyncPoint1;
		    ln_equ_posn ActualSyncPoint2;
		    double lstdegrees1 = (ln_get_apparent_sidereal_time(Entry1.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    double lstdegrees2 = (ln_get_apparent_sidereal_time(Entry2.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
		    ActualSyncPoint1.dec = RaDec1.dec;
		    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
		    ActualSyncPoint2.ra = (lstdegrees2 - RaDec2.ra);
		    ActualSyncPoint2.dec = RaDec2.dec;
		    ActualDirectionCosine2 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint2);

		    // geehalel: compute two matrices and use nearest point alignment
		    ln_equ_posn DummyRaDec;
		    TelescopeDirectionVector DummyActualDirectionCosine2;
		    TelescopeDirectionVector DummyApparentDirectionCosine2;
		    TelescopeDirectionVector DummyActualDirectionCosine3;
		    TelescopeDirectionVector DummyApparentDirectionCosine3;
		    gsl_matrix *R =          gsl_matrix_alloc(3,3);
		    gsl_vector *pActual1 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent1 = gsl_vector_alloc(3);
		    gsl_vector *pActual2 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent2 = gsl_vector_alloc(3);
		    // geehalel: first point
		    DummyRaDec.ra  = range24(Entry1.RightAscension + 6.0);
                    DummyRaDec.dec = Entry1.Declination;
		    DummyActualDirectionCosine2   = TelescopeDirectionVectorFromLocalHourAngleDeclination(DummyRaDec);
		    // geehalel: computes the rotation matrix from Actual1 to Apparent1
		    gsl_vector_set(pActual1, 0, ActualDirectionCosine1.x);
		    gsl_vector_set(pActual1, 1, ActualDirectionCosine1.y);
		    gsl_vector_set(pActual1, 2, ActualDirectionCosine1.z);
		    gsl_vector_set(pApparent1, 0, Entry1.TelescopeDirection.x);
		    gsl_vector_set(pApparent1, 1, Entry1.TelescopeDirection.y);
		    gsl_vector_set(pApparent1, 2, Entry1.TelescopeDirection.z);
		    RotationMatrixFromVectors(pActual1, pApparent1, R);
		    Dump3x3("Rot",R);
		    // geehale: and applies the rotation to Actual2 (Dummy point above)
		    gsl_vector_set(pActual2, 0, DummyActualDirectionCosine2.x);
		    gsl_vector_set(pActual2, 1, DummyActualDirectionCosine2.y);
		    gsl_vector_set(pActual2, 2, DummyActualDirectionCosine2.z);
		    MatrixVectorMultiply(R, pActual2, pApparent2);
		    DummyApparentDirectionCosine2.x = gsl_vector_get(pApparent2, 0);
		    DummyApparentDirectionCosine2.y = gsl_vector_get(pApparent2, 1);
		    DummyApparentDirectionCosine2.z = gsl_vector_get(pApparent2, 2);
		    DummyActualDirectionCosine3 = ActualDirectionCosine1 * DummyActualDirectionCosine2;
		    DummyActualDirectionCosine3.Normalise();
		    DummyApparentDirectionCosine3 = Entry1.TelescopeDirection * DummyApparentDirectionCosine2;
		    DummyApparentDirectionCosine3.Normalise();
		    CalculateTransformMatrices(ActualDirectionCosine1, DummyActualDirectionCosine2, DummyActualDirectionCosine3,
					       Entry1.TelescopeDirection, DummyApparentDirectionCosine2,
					       DummyApparentDirectionCosine3, pActualToApparentTransform,
					       pApparentToActualTransform);
		    // geehalel: second point
		    DummyRaDec.ra  = range24(Entry2.RightAscension + 6.0);
                    DummyRaDec.dec = Entry2.Declination;
		    DummyActualDirectionCosine2   = TelescopeDirectionVectorFromLocalHourAngleDeclination(DummyRaDec);
		    // geehalel: computes the rotation matrix from Actual1 to Apparent1
		    gsl_vector_set(pActual1, 0, ActualDirectionCosine2.x);
		    gsl_vector_set(pActual1, 1, ActualDirectionCosine2.y);
		    gsl_vector_set(pActual1, 2, ActualDirectionCosine2.z);
		    gsl_vector_set(pApparent1, 0, Entry2.TelescopeDirection.x);
		    gsl_vector_set(pApparent1, 1, Entry2.TelescopeDirection.y);
		    gsl_vector_set(pApparent1, 2, Entry2.TelescopeDirection.z);
		    RotationMatrixFromVectors(pActual1, pApparent1, R);
		    Dump3x3("Rot",R);
		    // geehale: and applies the rotation to Actual2 (Dummy point above)
		    gsl_vector_set(pActual2, 0, DummyActualDirectionCosine2.x);
		    gsl_vector_set(pActual2, 1, DummyActualDirectionCosine2.y);
		    gsl_vector_set(pActual2, 2, DummyActualDirectionCosine2.z);
		    MatrixVectorMultiply(R, pActual2, pApparent2);
		    DummyApparentDirectionCosine2.x = gsl_vector_get(pApparent2, 0);
		    DummyApparentDirectionCosine2.y = gsl_vector_get(pApparent2, 1);
		    DummyApparentDirectionCosine2.z = gsl_vector_get(pApparent2, 2);
		    DummyActualDirectionCosine3 = ActualDirectionCosine2 * DummyActualDirectionCosine2;
		    DummyActualDirectionCosine3.Normalise();
		    DummyApparentDirectionCosine3 = Entry2.TelescopeDirection * DummyApparentDirectionCosine2;
		    DummyApparentDirectionCosine3.Normalise();
		    CalculateTransformMatrices(ActualDirectionCosine2, DummyActualDirectionCosine2, DummyActualDirectionCosine3,
					       Entry2.TelescopeDirection, DummyApparentDirectionCosine2,
					       DummyApparentDirectionCosine3, pActualToApparentTransform_2,
					       pApparentToActualTransform_2);		    
		    
		    gsl_matrix_free(R);
		    gsl_vector_free(pActual1);
		    gsl_vector_free(pApparent1);
		    gsl_vector_free(pActual2);
		    gsl_vector_free(pApparent2);
		    break;
		}
	    }
	    /*
            TelescopeDirectionVector DummyActualDirectionCosine3;
            TelescopeDirectionVector DummyApparentDirectionCosine3;
            DummyActualDirectionCosine3 = ActualDirectionCosine1 * ActualDirectionCosine2;
            DummyActualDirectionCosine3.Normalise();
            DummyApparentDirectionCosine3 = Entry1.TelescopeDirection * Entry2.TelescopeDirection;
            DummyApparentDirectionCosine3.Normalise();

            // The third direction vectors is generated by taking the cross product of the first two
            CalculateTransformMatrices(ActualDirectionCosine1, ActualDirectionCosine2, DummyActualDirectionCosine3,
                                       Entry1.TelescopeDirection, Entry2.TelescopeDirection,
                                       DummyApparentDirectionCosine3, pActualToApparentTransform,
                                       pApparentToActualTransform);
	    */
            return true;
        }
	/*
        case 3:
        {
            // First compute local horizontal coordinates for the three sync points
            AlignmentDatabaseEntry &Entry1 = SyncPoints[0];
            AlignmentDatabaseEntry &Entry2 = SyncPoints[1];
            AlignmentDatabaseEntry &Entry3 = SyncPoints[2];
            ln_equ_posn RaDec1;
            ln_equ_posn RaDec2;
            ln_equ_posn RaDec3;
	    TelescopeDirectionVector ActualDirectionCosine1;
	    TelescopeDirectionVector ActualDirectionCosine2;
	    TelescopeDirectionVector ActualDirectionCosine3;
	    
            RaDec1.dec = Entry1.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec1.ra  = Entry1.RightAscension * 360.0 / 24.0;
            RaDec2.dec = Entry2.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec2.ra  = Entry2.RightAscension * 360.0 / 24.0;
            RaDec3.dec = Entry3.Declination;
            // libnova works in decimal degrees so conversion is needed here
            RaDec3.ra = Entry3.RightAscension * 360.0 / 24.0;
            ln_lnlat_posn Position { 0, 0 };
            if (!pInMemoryDatabase->GetDatabaseReferencePosition(Position))
                return false;
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		{
		    ln_hrz_posn ActualSyncPoint1;
		    ln_hrz_posn ActualSyncPoint2;
		    ln_hrz_posn ActualSyncPoint3;
		    ln_get_hrz_from_equ(&RaDec1, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
		    ln_get_hrz_from_equ(&RaDec2, &Position, Entry2.ObservationJulianDate, &ActualSyncPoint2);
		    ln_get_hrz_from_equ(&RaDec3, &Position, Entry3.ObservationJulianDate, &ActualSyncPoint3);
		    // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
		    ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
		    ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);
		    ActualDirectionCosine3 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint3);
		    break;
		}
	        case NORTH_CELESTIAL_POLE:
	        case SOUTH_CELESTIAL_POLE:
                {
		    ln_equ_posn ActualSyncPoint1;
		    ln_equ_posn ActualSyncPoint2;
		    ln_equ_posn ActualSyncPoint3;
		    double lstdegrees1 = (ln_get_apparent_sidereal_time(Entry1.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    double lstdegrees2 = (ln_get_apparent_sidereal_time(Entry2.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    double lstdegrees3 = (ln_get_apparent_sidereal_time(Entry3.ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
		    ActualSyncPoint1.dec = RaDec1.dec;
		    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
		    ActualSyncPoint2.ra = (lstdegrees2 - RaDec2.ra);
		    ActualSyncPoint2.dec = RaDec2.dec;
		    ActualDirectionCosine2 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint2);
		    ActualSyncPoint3.ra = (lstdegrees3 - RaDec3.ra);
		    ActualSyncPoint3.dec = RaDec3.dec;
		    ActualDirectionCosine3 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint3);
		    break;
		}
	    }
 

            CalculateTransformMatrices(ActualDirectionCosine1, ActualDirectionCosine2, ActualDirectionCosine3,
                                       Entry1.TelescopeDirection, Entry2.TelescopeDirection, Entry3.TelescopeDirection,
                                       pActualToApparentTransform, pApparentToActualTransform);
            return true;
        }
	*/
        default:
        {
            ln_lnlat_posn Position { 0, 0 };
            if (!pInMemoryDatabase->GetDatabaseReferencePosition(Position))
                return false;

            // Compute Hulls etc.
            ActualConvexHull.Reset();
            ApparentConvexHull.Reset();
            ActualDirectionCosines.clear();

            // Add a dummy point at the nadir
            ActualConvexHull.MakeNewVertex(0.0, 0.0, -1.0, 0);
            ApparentConvexHull.MakeNewVertex(0.0, 0.0, -1.0, 0);

            int VertexNumber = 1;
            // Add the rest of the vertices
            for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
                 Itr != SyncPoints.end(); Itr++)
            {
                ln_equ_posn RaDec;
		TelescopeDirectionVector ActualDirectionCosine;
                RaDec.dec = (*Itr).Declination;
                // libnova works in decimal degrees so conversion is needed here
                RaDec.ra = (*Itr).RightAscension * 360.0 / 24.0;
		switch (ApproximateMountAlignment)
		{
                    case ZENITH:
		    {
		        ln_hrz_posn ActualSyncPoint;
		        ln_get_hrz_from_equ(&RaDec, &Position, (*Itr).ObservationJulianDate, &ActualSyncPoint);
			// Now express this coordinate as normalised direction vectors (a.k.a direction cosines)
			ActualDirectionCosine = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint);
			break;
		    }
		    case NORTH_CELESTIAL_POLE:
	            case SOUTH_CELESTIAL_POLE:
		    {
		        ln_equ_posn ActualSyncPoint;
			double lstdegrees = (ln_get_apparent_sidereal_time((*Itr).ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			ActualSyncPoint.ra = (lstdegrees - RaDec.ra);
			ActualSyncPoint.dec = RaDec.dec;
			ActualDirectionCosine =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint);
			break;
		    }
		}
		ActualDirectionCosines.push_back(ActualDirectionCosine);
                ActualConvexHull.MakeNewVertex(ActualDirectionCosine.x, ActualDirectionCosine.y,
                                               ActualDirectionCosine.z, VertexNumber);
                ApparentConvexHull.MakeNewVertex((*Itr).TelescopeDirection.x, (*Itr).TelescopeDirection.y,
                                                 (*Itr).TelescopeDirection.z, VertexNumber);
                VertexNumber++;
            }
            // I should only need to do this once but it is easier to do it twice
            ActualConvexHull.DoubleTriangle();
            ActualConvexHull.ConstructHull();
            ActualConvexHull.EdgeOrderOnFaces();
            ApparentConvexHull.DoubleTriangle();
            ApparentConvexHull.ConstructHull();
            ApparentConvexHull.EdgeOrderOnFaces();

            // Make the matrices
            ConvexHull::tFace CurrentFace = ActualConvexHull.faces;
#ifdef CONVEX_HULL_DEBUGGING
            int ActualFaces = 0;
#endif
            if (nullptr != CurrentFace)
            {
                do
                {
#ifdef CONVEX_HULL_DEBUGGING
                    ActualFaces++;
#endif
                    if ((0 == CurrentFace->vertex[0]->vnum) || (0 == CurrentFace->vertex[1]->vnum) ||
                        (0 == CurrentFace->vertex[2]->vnum))
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Initialise - Ignoring actual face %d", ActualFaces);
#endif
                    }
                    else
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Initialise - Processing actual face %d v1 %d v2 %d v3 %d", ActualFaces,
                                  CurrentFace->vertex[0]->vnum, CurrentFace->vertex[1]->vnum,
                                  CurrentFace->vertex[2]->vnum);
#endif
                        CalculateTransformMatrices(ActualDirectionCosines[CurrentFace->vertex[0]->vnum - 1],
                                                   ActualDirectionCosines[CurrentFace->vertex[1]->vnum - 1],
                                                   ActualDirectionCosines[CurrentFace->vertex[2]->vnum - 1],
                                                   SyncPoints[CurrentFace->vertex[0]->vnum - 1].TelescopeDirection,
                                                   SyncPoints[CurrentFace->vertex[1]->vnum - 1].TelescopeDirection,
                                                   SyncPoints[CurrentFace->vertex[2]->vnum - 1].TelescopeDirection,
                                                   CurrentFace->pMatrix, nullptr);
                    }
                    CurrentFace = CurrentFace->next;
                } while (CurrentFace != ActualConvexHull.faces);
            }

            // One of these days I will optimise this
            CurrentFace = ApparentConvexHull.faces;
#ifdef CONVEX_HULL_DEBUGGING
            int ApparentFaces = 0;
#endif
            if (nullptr != CurrentFace)
            {
                do
                {
#ifdef CONVEX_HULL_DEBUGGING
                    ApparentFaces++;
#endif
                    if ((0 == CurrentFace->vertex[0]->vnum) || (0 == CurrentFace->vertex[1]->vnum) ||
                        (0 == CurrentFace->vertex[2]->vnum))
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Initialise - Ignoring apparent face %d", ApparentFaces);
#endif
                    }
                    else
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Initialise - Processing apparent face %d v1 %d v2 %d v3 %d", ApparentFaces,
                                  CurrentFace->vertex[0]->vnum, CurrentFace->vertex[1]->vnum,
                                  CurrentFace->vertex[2]->vnum);
#endif
                        CalculateTransformMatrices(SyncPoints[CurrentFace->vertex[0]->vnum - 1].TelescopeDirection,
                                                   SyncPoints[CurrentFace->vertex[1]->vnum - 1].TelescopeDirection,
                                                   SyncPoints[CurrentFace->vertex[2]->vnum - 1].TelescopeDirection,
                                                   ActualDirectionCosines[CurrentFace->vertex[0]->vnum - 1],
                                                   ActualDirectionCosines[CurrentFace->vertex[1]->vnum - 1],
                                                   ActualDirectionCosines[CurrentFace->vertex[2]->vnum - 1],
                                                   CurrentFace->pMatrix, nullptr);
                    }
                    CurrentFace = CurrentFace->next;
                } while (CurrentFace != ApparentConvexHull.faces);
            }

#ifdef CONVEX_HULL_DEBUGGING
            ASSDEBUGF("Initialise - ActualFaces %d ApparentFaces %d", ActualFaces, ApparentFaces);
            ActualConvexHull.PrintObj("ActualHull.obj");
            ActualConvexHull.PrintOut("ActualHull.log", ActualConvexHull.vertices);
            ApparentConvexHull.PrintObj("ApparentHull.obj");
            ActualConvexHull.PrintOut("ApparentHull.log", ApparentConvexHull.vertices);
#endif
            return true;
        }
    }
}

bool BasicMathPlugin::TransformCelestialToTelescope(const double RightAscension, const double Declination,
                                                    double JulianOffset,
                                                    TelescopeDirectionVector &ApparentTelescopeDirectionVector)
{
    ln_equ_posn ActualRaDec;
    TelescopeDirectionVector ActualVector;
    
    // libnova works in decimal degrees so conversion is needed here
    ActualRaDec.ra  = RightAscension * 360.0 / 24.0;
    ActualRaDec.dec = Declination;
    ln_lnlat_posn Position { 0, 0 };

    if ((nullptr == pInMemoryDatabase) ||
        !pInMemoryDatabase->GetDatabaseReferencePosition(
            Position)) // Should check that this the same as the current observing position
        return false;

    switch (ApproximateMountAlignment)
    {
        case ZENITH:
	    ln_hrz_posn ActualAltAz;
	    ln_get_hrz_from_equ(&ActualRaDec, &Position, ln_get_julian_from_sys() + JulianOffset, &ActualAltAz);
	    ASSDEBUGF("Celestial to telescope - Actual Alt %lf Az %lf", ActualAltAz.alt, ActualAltAz.az);
	    ActualVector = TelescopeDirectionVectorFromAltitudeAzimuth(ActualAltAz);
	    break;
        case NORTH_CELESTIAL_POLE:
        case SOUTH_CELESTIAL_POLE:
	    ln_equ_posn ActualHourAngleDec;
	    double lstdegrees = (ln_get_apparent_sidereal_time(ln_get_julian_from_sys() + JulianOffset) * 360.0 / 24.0) + Position.lng;
	    ActualHourAngleDec.ra = (lstdegrees - ActualRaDec.ra);
	    ActualHourAngleDec.dec = ActualRaDec.dec;
	    ActualVector =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualHourAngleDec);
	    break;
    }
    

    InMemoryDatabase::AlignmentDatabaseType &SyncPoints = pInMemoryDatabase->GetAlignmentDatabase();
    switch (SyncPoints.size())
    {
        case 0:
        {
            // 0 sync points
            ApparentTelescopeDirectionVector = ActualVector;

            break;
        }
        case 1:
	/* geehalel: case 2:use nearest, case 3 use default
        case 2:
        case 3:
	*/
        {
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLActualVector, 0, ActualVector.x);
            gsl_vector_set(pGSLActualVector, 1, ActualVector.y);
            gsl_vector_set(pGSLActualVector, 2, ActualVector.z);
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pActualToApparentTransform, pGSLActualVector, pGSLApparentVector);
            ApparentTelescopeDirectionVector.x = gsl_vector_get(pGSLApparentVector, 0);
            ApparentTelescopeDirectionVector.y = gsl_vector_get(pGSLApparentVector, 1);
            ApparentTelescopeDirectionVector.z = gsl_vector_get(pGSLApparentVector, 2);
            ApparentTelescopeDirectionVector.Normalise();
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            break;
        }
        case 2:
	{
	    // geehalel: Find the nearest point and use that transform
	    std::map<double, const AlignmentDatabaseEntry *> NearestMap;
	    for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
		 Itr != SyncPoints.end(); Itr++)
	    {
	        ln_equ_posn RaDec;
		TelescopeDirectionVector ActualDirectionCosine;
		RaDec.ra  = (*Itr).RightAscension * 360.0 / 24.0;
		RaDec.dec = (*Itr).Declination;
		switch (ApproximateMountAlignment)
		{
		case ZENITH:
		{
		    ln_hrz_posn ActualPoint;
		    ln_get_hrz_from_equ(&RaDec, &Position, (*Itr).ObservationJulianDate, &ActualPoint);
		    // Now express this coordinate as normalised direction vectors (a.k.a direction cosines)
		    ActualDirectionCosine = TelescopeDirectionVectorFromAltitudeAzimuth(ActualPoint);
		    break;
		}
		case NORTH_CELESTIAL_POLE:
		case SOUTH_CELESTIAL_POLE:
		{
		    ln_equ_posn ActualPoint;
		    double lstdegrees = (ln_get_apparent_sidereal_time((*Itr).ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualPoint.ra = (lstdegrees - RaDec.ra);
		    ActualPoint.dec = RaDec.dec;
		    ActualDirectionCosine =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualPoint);
		    break;
		}
		}
		
		// geehalel: use great circle  distance https://en.wikipedia.org/wiki/Great-circle_distance#Vector_version
		//NearestMap[(ActualDirectionCosine - ActualVector).Length()] = &(*Itr);
		NearestMap[atan2((ActualDirectionCosine * ActualVector).Length(), ActualDirectionCosine ^ ActualVector)] = &(*Itr);
	    }
	    // geehalel: Get the nearest transform
	    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
	    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
	    gsl_matrix *pTransform;
	    if (pEntry1 == &SyncPoints[0]) pTransform = pActualToApparentTransform; else pTransform = pActualToApparentTransform_2;
	    gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLActualVector, 0, ActualVector.x);
            gsl_vector_set(pGSLActualVector, 1, ActualVector.y);
            gsl_vector_set(pGSLActualVector, 2, ActualVector.z);
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pTransform, pGSLActualVector, pGSLApparentVector);
            ApparentTelescopeDirectionVector.x = gsl_vector_get(pGSLApparentVector, 0);
            ApparentTelescopeDirectionVector.y = gsl_vector_get(pGSLApparentVector, 1);
            ApparentTelescopeDirectionVector.z = gsl_vector_get(pGSLApparentVector, 2);
            ApparentTelescopeDirectionVector.Normalise();
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
	    break;
	}
        default:
        {
            gsl_matrix *pTransform;
            gsl_matrix *pComputedTransform = nullptr;
            // Scale the actual telescope direction vector to make sure it traverses the unit sphere.
            TelescopeDirectionVector ScaledActualVector = ActualVector * 2.0;
            // Shoot the scaled vector in the into the list of actual facets
            // and use the conversuion matrix from the one it intersects
            ConvexHull::tFace CurrentFace = ActualConvexHull.faces;
#ifdef CONVEX_HULL_DEBUGGING
            int ActualFaces = 0;
#endif
            if (nullptr != CurrentFace)
            {
                do
                {
#ifdef CONVEX_HULL_DEBUGGING
                    ActualFaces++;
#endif
                    // Ignore faces containg vertex 0 (nadir).
                    if ((0 == CurrentFace->vertex[0]->vnum) || (0 == CurrentFace->vertex[1]->vnum) ||
                        (0 == CurrentFace->vertex[2]->vnum))
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Celestial to telescope - Ignoring actual face %d", ActualFaces);
#endif
                    }
                    else
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Celestial to telescope - Processing actual face %d v1 %d v2 %d v3 %d", ActualFaces,
                                  CurrentFace->vertex[0]->vnum, CurrentFace->vertex[1]->vnum,
                                  CurrentFace->vertex[2]->vnum);
#endif
                        if (RayTriangleIntersection(ScaledActualVector,
                                                    ActualDirectionCosines[CurrentFace->vertex[0]->vnum - 1],
                                                    ActualDirectionCosines[CurrentFace->vertex[1]->vnum - 1],
                                                    ActualDirectionCosines[CurrentFace->vertex[2]->vnum - 1]))
                            break;
                    }
                    CurrentFace = CurrentFace->next;
                } while (CurrentFace != ActualConvexHull.faces);
                if (CurrentFace == ActualConvexHull.faces)
                {
                    // Find the three nearest points and build a transform
                    std::map<double, const AlignmentDatabaseEntry *> NearestMap;
                    for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
                         Itr != SyncPoints.end(); Itr++)
                    {
                        ln_equ_posn RaDec;
			TelescopeDirectionVector ActualDirectionCosine;
                        RaDec.ra  = (*Itr).RightAscension * 360.0 / 24.0;
                        RaDec.dec = (*Itr).Declination;
			switch (ApproximateMountAlignment)
			{
			    case ZENITH:
			    {
			        ln_hrz_posn ActualPoint;
				ln_get_hrz_from_equ(&RaDec, &Position, (*Itr).ObservationJulianDate, &ActualPoint);
				// Now express this coordinate as normalised direction vectors (a.k.a direction cosines)
				ActualDirectionCosine = TelescopeDirectionVectorFromAltitudeAzimuth(ActualPoint);
				break;
			    }
			    case NORTH_CELESTIAL_POLE:
			    case SOUTH_CELESTIAL_POLE:
			    {
			        ln_equ_posn ActualPoint;
				double lstdegrees = (ln_get_apparent_sidereal_time((*Itr).ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
				ActualPoint.ra = (lstdegrees - RaDec.ra);
				ActualPoint.dec = RaDec.dec;
				ActualDirectionCosine =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualPoint);
				break;
			    }
			}
			// geehalel: use great circle  distance https://en.wikipedia.org/wiki/Great-circle_distance#Vector_version
			//NearestMap[(ActualDirectionCosine - ActualVector).Length()] = &(*Itr);
			NearestMap[atan2((ActualDirectionCosine * ActualVector).Length(), ActualDirectionCosine ^ ActualVector)] = &(*Itr);
                    }
		    /* geehalel: use a nearest algorithm
                    // First compute local horizontal coordinates for the three sync points
                    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
                    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
                    Nearest++;
                    const AlignmentDatabaseEntry *pEntry2 = (*Nearest).second;
                    Nearest++;
                    const AlignmentDatabaseEntry *pEntry3 = (*Nearest).second;

		    ln_equ_posn RaDec1;
                    ln_equ_posn RaDec2;
                    ln_equ_posn RaDec3;		    
                    RaDec1.dec = pEntry1->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec1.ra  = pEntry1->RightAscension * 360.0 / 24.0;
                    RaDec2.dec = pEntry2->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec2.ra  = pEntry2->RightAscension * 360.0 / 24.0;
                    RaDec3.dec = pEntry3->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec3.ra = pEntry3->RightAscension * 360.0 / 24.0;

		    TelescopeDirectionVector ActualDirectionCosine1;
		    TelescopeDirectionVector ActualDirectionCosine2;
		    TelescopeDirectionVector ActualDirectionCosine3;
		    switch (ApproximateMountAlignment)
		    {
		        case ZENITH:
			{
			    ln_hrz_posn ActualSyncPoint1;
			    ln_hrz_posn ActualSyncPoint2;
			    ln_hrz_posn ActualSyncPoint3;
			    ln_get_hrz_from_equ(&RaDec1, &Position, pEntry1->ObservationJulianDate, &ActualSyncPoint1);
			    ln_get_hrz_from_equ(&RaDec2, &Position, pEntry2->ObservationJulianDate, &ActualSyncPoint2);
			    ln_get_hrz_from_equ(&RaDec3, &Position, pEntry3->ObservationJulianDate, &ActualSyncPoint3);
			    // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
			    ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
			    ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);
			    ActualDirectionCosine3 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint3);
			    break;
			}
		        case NORTH_CELESTIAL_POLE:
		        case SOUTH_CELESTIAL_POLE:
			{
			    ln_equ_posn ActualSyncPoint1;
			    ln_equ_posn ActualSyncPoint2;
			    ln_equ_posn ActualSyncPoint3;
			    double lstdegrees1 = (ln_get_apparent_sidereal_time(pEntry1->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    double lstdegrees2 = (ln_get_apparent_sidereal_time(pEntry2->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    double lstdegrees3 = (ln_get_apparent_sidereal_time(pEntry3->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
			    ActualSyncPoint1.dec = RaDec1.dec;
			    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
			    ActualSyncPoint2.ra = (lstdegrees2 - RaDec2.ra);
			    ActualSyncPoint2.dec = RaDec2.dec;
			    ActualDirectionCosine2 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint2);
			    ActualSyncPoint3.ra = (lstdegrees3 - RaDec3.ra);
			    ActualSyncPoint3.dec = RaDec3.dec;
			    ActualDirectionCosine3 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint3);
			    break;
			}
		    }
		    pComputedTransform = gsl_matrix_alloc(3, 3);
                    CalculateTransformMatrices(ActualDirectionCosine1, ActualDirectionCosine2, ActualDirectionCosine3,
                                               pEntry1->TelescopeDirection, pEntry2->TelescopeDirection,
                                               pEntry3->TelescopeDirection, pComputedTransform, nullptr);
		    */
		    // geehalel Nearest
		    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
                    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
		    ln_equ_posn DummyRaDec;
		    ln_equ_posn ActualSyncPoint1;		    
		    TelescopeDirectionVector ActualDirectionCosine1;
		    ln_equ_posn RaDec1;
		    RaDec1.dec = pEntry1->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec1.ra  = pEntry1->RightAscension * 360.0 / 24.0;
		    double lstdegrees1 = (ln_get_apparent_sidereal_time(pEntry1->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
		    ActualSyncPoint1.dec = RaDec1.dec;
		    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
		    TelescopeDirectionVector DummyActualDirectionCosine2;
		    TelescopeDirectionVector DummyApparentDirectionCosine2;
		    TelescopeDirectionVector DummyActualDirectionCosine3;
		    TelescopeDirectionVector DummyApparentDirectionCosine3;
		    gsl_matrix *R =          gsl_matrix_alloc(3,3);
		    gsl_vector *pActual1 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent1 = gsl_vector_alloc(3);
		    gsl_vector *pActual2 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent2 = gsl_vector_alloc(3);
		    DummyRaDec.ra  = range24(pEntry1->RightAscension + 6.0);
                    DummyRaDec.dec = pEntry1->Declination;
		    DummyActualDirectionCosine2   = TelescopeDirectionVectorFromLocalHourAngleDeclination(DummyRaDec);
		    // geehalel: computes the rotation matrix from Actual1 to Apparent1
		    gsl_vector_set(pActual1, 0, ActualDirectionCosine1.x);
		    gsl_vector_set(pActual1, 1, ActualDirectionCosine1.y);
		    gsl_vector_set(pActual1, 2, ActualDirectionCosine1.z);
		    gsl_vector_set(pApparent1, 0, pEntry1->TelescopeDirection.x);
		    gsl_vector_set(pApparent1, 1, pEntry1->TelescopeDirection.y);
		    gsl_vector_set(pApparent1, 2, pEntry1->TelescopeDirection.z);
		    RotationMatrixFromVectors(pActual1, pApparent1, R);
		    Dump3x3("Rot",R);
		    // geehale: and applies the rotation to Actual2 (Dummy point above)
		    gsl_vector_set(pActual2, 0, DummyActualDirectionCosine2.x);
		    gsl_vector_set(pActual2, 1, DummyActualDirectionCosine2.y);
		    gsl_vector_set(pActual2, 2, DummyActualDirectionCosine2.z);
		    MatrixVectorMultiply(R, pActual2, pApparent2);
		    DummyApparentDirectionCosine2.x = gsl_vector_get(pApparent2, 0);
		    DummyApparentDirectionCosine2.y = gsl_vector_get(pApparent2, 1);
		    DummyApparentDirectionCosine2.z = gsl_vector_get(pApparent2, 2);
		    DummyActualDirectionCosine3 = ActualDirectionCosine1 * DummyActualDirectionCosine2;
		    DummyActualDirectionCosine3.Normalise();
		    DummyApparentDirectionCosine3 = pEntry1->TelescopeDirection * DummyApparentDirectionCosine2;
		    DummyApparentDirectionCosine3.Normalise();
		    pComputedTransform = gsl_matrix_alloc(3, 3);
		    CalculateTransformMatrices(ActualDirectionCosine1, DummyActualDirectionCosine2, DummyActualDirectionCosine3,
					       pEntry1->TelescopeDirection, DummyApparentDirectionCosine2,
					       DummyApparentDirectionCosine3, pComputedTransform,
					       nullptr);		    
		    // geehalel Nearest
                    pTransform = pComputedTransform;
                }
                else
                    pTransform = CurrentFace->pMatrix;
            }
            else
                return false;

            // OK - got an intersection - CurrentFace is pointing at the face
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLActualVector, 0, ActualVector.x);
            gsl_vector_set(pGSLActualVector, 1, ActualVector.y);
            gsl_vector_set(pGSLActualVector, 2, ActualVector.z);
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pTransform, pGSLActualVector, pGSLApparentVector);
            ApparentTelescopeDirectionVector.x = gsl_vector_get(pGSLApparentVector, 0);
            ApparentTelescopeDirectionVector.y = gsl_vector_get(pGSLApparentVector, 1);
            ApparentTelescopeDirectionVector.z = gsl_vector_get(pGSLApparentVector, 2);
            ApparentTelescopeDirectionVector.Normalise();
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            if (nullptr != pComputedTransform)
                gsl_matrix_free(pComputedTransform);
            break;
        }
    }

    ln_hrz_posn ApparentAltAz;
    AltitudeAzimuthFromTelescopeDirectionVector(ApparentTelescopeDirectionVector, ApparentAltAz);
    ASSDEBUGF("Celestial to telescope - Apparent Alt %lf Az %lf", ApparentAltAz.alt, ApparentAltAz.az);

    return true;
}

bool BasicMathPlugin::TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
                                                    double &RightAscension, double &Declination)
{
    ln_lnlat_posn Position { 0, 0 };
    ln_equ_posn ActualRaDec;

    ASSDEBUGF("Telescope to celestial - ApparentVector x %lf y %lf z %lf", ApparentTelescopeDirectionVector.x,
                      ApparentTelescopeDirectionVector.y, ApparentTelescopeDirectionVector.z);
    
    if ((nullptr == pInMemoryDatabase) || !pInMemoryDatabase->GetDatabaseReferencePosition(Position))
    {
        // Should check that this the same as the current observing position
        ASSDEBUG("No database or no position in database");
        return false;
    }
    InMemoryDatabase::AlignmentDatabaseType &SyncPoints = pInMemoryDatabase->GetAlignmentDatabase();
    switch (SyncPoints.size())
    {
        case 0:
        {
            // 0 sync points: ActualTelescopeDirectionVector equals ApparentTelescopeDirectionVector
	    TelescopeDirectionVector ActualTelescopeDirectionVector(ApparentTelescopeDirectionVector);
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		{
		    ln_hrz_posn ActualAltAz;
		    AltitudeAzimuthFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualAltAz);
                    ln_get_equ_from_hrz(&ActualAltAz, &Position, ln_get_julian_from_sys(), &ActualRaDec);
                    break;
		}
                case NORTH_CELESTIAL_POLE:
                case SOUTH_CELESTIAL_POLE:
		{
		    ln_equ_posn ActualHourAngleDec;
		    double lstdegrees = (ln_get_apparent_sidereal_time(ln_get_julian_from_sys()) * 360.0 / 24.0) + Position.lng;
		    LocalHourAngleDeclinationFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualHourAngleDec);
		    ActualRaDec.ra= (lstdegrees - ActualHourAngleDec.ra);
		    ActualRaDec.dec =  ActualHourAngleDec.dec;
                    break;
		}
            }
            // libnova works in decimal degrees so conversion is needed here
            RightAscension = ActualRaDec.ra * 24.0 / 360.0;
            Declination    = ActualRaDec.dec;
            break;
        }
        case 1:
	/* geehalel uses nearest for 2 points case and convex hull for 3 point case
        case 2:
        case 3:
	*/
        {
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLApparentVector, 0, ApparentTelescopeDirectionVector.x);
            gsl_vector_set(pGSLApparentVector, 1, ApparentTelescopeDirectionVector.y);
            gsl_vector_set(pGSLApparentVector, 2, ApparentTelescopeDirectionVector.z);
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pApparentToActualTransform, pGSLApparentVector, pGSLActualVector);

            Dump3("ApparentVector", pGSLApparentVector);
            Dump3("ActualVector", pGSLActualVector);

            TelescopeDirectionVector ActualTelescopeDirectionVector;
            ActualTelescopeDirectionVector.x = gsl_vector_get(pGSLActualVector, 0);
            ActualTelescopeDirectionVector.y = gsl_vector_get(pGSLActualVector, 1);
            ActualTelescopeDirectionVector.z = gsl_vector_get(pGSLActualVector, 2);
            ActualTelescopeDirectionVector.Normalise();
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		    ln_hrz_posn ActualAltAz;
		    AltitudeAzimuthFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualAltAz);
		    ln_get_equ_from_hrz(&ActualAltAz, &Position, ln_get_julian_from_sys(), &ActualRaDec);
		    break;
	        case NORTH_CELESTIAL_POLE:
                case SOUTH_CELESTIAL_POLE:
		    ln_equ_posn ActualHourAngleDec;
		    double lstdegrees = (ln_get_apparent_sidereal_time(ln_get_julian_from_sys()) * 360.0 / 24.0) + Position.lng;
		    LocalHourAngleDeclinationFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualHourAngleDec);
		    ActualRaDec.ra= (lstdegrees - ActualHourAngleDec.ra);
		    ActualRaDec.dec =  ActualHourAngleDec.dec;
                    break;
	    }
            // libnova works in decimal degrees so conversion is needed here
            RightAscension = ActualRaDec.ra * 24.0 / 360.0;
            Declination    = ActualRaDec.dec;
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            break;
        }
        case 2:
	{
	    // geehalel: Find the nearest apparent point and use that transform
	    std::map<double, const AlignmentDatabaseEntry *> NearestMap;
	    for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
		 Itr != SyncPoints.end(); Itr++)
	    {	

		// geehalel: use great circle  distance https://en.wikipedia.org/wiki/Great-circle_distance#Vector_version
		//NearestMap[((*Itr).TelescopeDirection - ApparentTelescopeDirectionVector).Length()] = &(*Itr);
		NearestMap[atan2(((*Itr).TelescopeDirection * ApparentTelescopeDirectionVector).Length(),
				 (*Itr).TelescopeDirection ^ ApparentTelescopeDirectionVector)] = &(*Itr);
	    }
	    // geehalel: Get the nearest transform
	    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
	    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
	    gsl_matrix *pTransform;
	    if (pEntry1 == &SyncPoints[0]) pTransform = pApparentToActualTransform; else pTransform = pApparentToActualTransform_2;
	    gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLApparentVector, 0, ApparentTelescopeDirectionVector.x);
            gsl_vector_set(pGSLApparentVector, 1, ApparentTelescopeDirectionVector.y);
            gsl_vector_set(pGSLApparentVector, 2, ApparentTelescopeDirectionVector.z);
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pTransform, pGSLApparentVector, pGSLActualVector);
	    TelescopeDirectionVector ActualTelescopeDirectionVector;
            ActualTelescopeDirectionVector.x = gsl_vector_get(pGSLActualVector, 0);
            ActualTelescopeDirectionVector.y = gsl_vector_get(pGSLActualVector, 1);
            ActualTelescopeDirectionVector.z = gsl_vector_get(pGSLActualVector, 2);
            ActualTelescopeDirectionVector.Normalise();
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		    ln_hrz_posn ActualAltAz;
		    AltitudeAzimuthFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualAltAz);
		    ln_get_equ_from_hrz(&ActualAltAz, &Position, ln_get_julian_from_sys(), &ActualRaDec);
		    break;
	        case NORTH_CELESTIAL_POLE:
                case SOUTH_CELESTIAL_POLE:
		    ln_equ_posn ActualHourAngleDec;
		    double lstdegrees = (ln_get_apparent_sidereal_time(ln_get_julian_from_sys()) * 360.0 / 24.0) + Position.lng;
		    LocalHourAngleDeclinationFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualHourAngleDec);
		    ActualRaDec.ra= (lstdegrees - ActualHourAngleDec.ra);
		    ActualRaDec.dec =  ActualHourAngleDec.dec;
                    break;
	    }
            // libnova works in decimal degrees so conversion is needed here
            RightAscension = ActualRaDec.ra * 24.0 / 360.0;
            Declination    = ActualRaDec.dec;	    
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
	    break;
	}
        default:
        {
            gsl_matrix *pTransform;
            gsl_matrix *pComputedTransform = nullptr;
            // Scale the apparent telescope direction vector to make sure it traverses the unit sphere.
            TelescopeDirectionVector ScaledApparentVector = ApparentTelescopeDirectionVector * 2.0;
            // Shoot the scaled vector in the into the list of apparent facets
            // and use the conversuion matrix from the one it intersects
            ConvexHull::tFace CurrentFace = ApparentConvexHull.faces;
#ifdef CONVEX_HULL_DEBUGGING
            int ApparentFaces = 0;
#endif
            if (nullptr != CurrentFace)
            {
                do
                {
#ifdef CONVEX_HULL_DEBUGGING
                    ApparentFaces++;
#endif
                    // Ignore faces containg vertex 0 (nadir).
                    if ((0 == CurrentFace->vertex[0]->vnum) || (0 == CurrentFace->vertex[1]->vnum) ||
                        (0 == CurrentFace->vertex[2]->vnum))
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("Celestial to telescope - Ignoring apparent face %d", ApparentFaces);
#endif
                    }
                    else
                    {
#ifdef CONVEX_HULL_DEBUGGING
                        ASSDEBUGF("TelescopeToCelestial - Processing apparent face %d v1 %d v2 %d v3 %d", ApparentFaces,
                                  CurrentFace->vertex[0]->vnum, CurrentFace->vertex[1]->vnum,
                                  CurrentFace->vertex[2]->vnum);
#endif
                        if (RayTriangleIntersection(ScaledApparentVector,
                                                    SyncPoints[CurrentFace->vertex[0]->vnum - 1].TelescopeDirection,
                                                    SyncPoints[CurrentFace->vertex[1]->vnum - 1].TelescopeDirection,
                                                    SyncPoints[CurrentFace->vertex[2]->vnum - 1].TelescopeDirection))
                            break;
                    }
                    CurrentFace = CurrentFace->next;
                } while (CurrentFace != ApparentConvexHull.faces);
                if (CurrentFace == ApparentConvexHull.faces)
                {
		    /* geehalel; use a nearest algorithm
                    // Find the three nearest points and build a transform
                    std::map<double, const AlignmentDatabaseEntry *> NearestMap;
                    for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
                         Itr != SyncPoints.end(); Itr++)
                    {
                        NearestMap[((*Itr).TelescopeDirection - ApparentTelescopeDirectionVector).Length()] = &(*Itr);
                    }
                    // First compute local horizontal coordinates for the three sync points
                    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
                    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
                    Nearest++;
                    const AlignmentDatabaseEntry *pEntry2 = (*Nearest).second;
                    Nearest++;
                    const AlignmentDatabaseEntry *pEntry3 = (*Nearest).second;

                    ln_equ_posn RaDec1;
                    ln_equ_posn RaDec2;
                    ln_equ_posn RaDec3;
                    RaDec1.dec = pEntry1->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec1.ra  = pEntry1->RightAscension * 360.0 / 24.0;
                    RaDec2.dec = pEntry2->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec2.ra  = pEntry2->RightAscension * 360.0 / 24.0;
                    RaDec3.dec = pEntry3->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec3.ra = pEntry3->RightAscension * 360.0 / 24.0;

		    TelescopeDirectionVector ActualDirectionCosine1;
		    TelescopeDirectionVector ActualDirectionCosine2;
		    TelescopeDirectionVector ActualDirectionCosine3;
		    switch (ApproximateMountAlignment)
		    {
		        case ZENITH:
			{
			    ln_hrz_posn ActualSyncPoint1;
			    ln_hrz_posn ActualSyncPoint2;
			    ln_hrz_posn ActualSyncPoint3;
			    ln_get_hrz_from_equ(&RaDec1, &Position, pEntry1->ObservationJulianDate, &ActualSyncPoint1);
			    ln_get_hrz_from_equ(&RaDec2, &Position, pEntry2->ObservationJulianDate, &ActualSyncPoint2);
			    ln_get_hrz_from_equ(&RaDec3, &Position, pEntry3->ObservationJulianDate, &ActualSyncPoint3);
			    // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
			    ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
			    ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);
			    ActualDirectionCosine3 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint3);
			    break;
			}
		        case NORTH_CELESTIAL_POLE:
		        case SOUTH_CELESTIAL_POLE:
			{
			    ln_equ_posn ActualSyncPoint1;
			    ln_equ_posn ActualSyncPoint2;
			    ln_equ_posn ActualSyncPoint3;
			    double lstdegrees1 = (ln_get_apparent_sidereal_time(pEntry1->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    double lstdegrees2 = (ln_get_apparent_sidereal_time(pEntry2->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    double lstdegrees3 = (ln_get_apparent_sidereal_time(pEntry3->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
			    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
			    ActualSyncPoint1.dec = RaDec1.dec;
			    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
			    ActualSyncPoint2.ra = (lstdegrees2 - RaDec2.ra);
			    ActualSyncPoint2.dec = RaDec2.dec;
			    ActualDirectionCosine2 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint2);
			    ActualSyncPoint3.ra = (lstdegrees3 - RaDec3.ra);
			    ActualSyncPoint3.dec = RaDec3.dec;
			    ActualDirectionCosine3 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint3);
			    break;
			}
		    }

                    pComputedTransform = gsl_matrix_alloc(3, 3);
                    CalculateTransformMatrices(pEntry1->TelescopeDirection, pEntry2->TelescopeDirection,
                                               pEntry3->TelescopeDirection, ActualDirectionCosine1,
                                               ActualDirectionCosine2, ActualDirectionCosine3, pComputedTransform,
                                               nullptr);
		    */
		    // geehalel: Find the nearest apparent point and build a transform
		    std::map<double, const AlignmentDatabaseEntry *> NearestMap;
                    for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin();
                         Itr != SyncPoints.end(); Itr++)
                    {
			// geehalel: use great circle  distance https://en.wikipedia.org/wiki/Great-circle_distance#Vector_version
			//NearestMap[((*Itr).TelescopeDirection - ApparentTelescopeDirectionVector).Length()] = &(*Itr);
			NearestMap[atan2(((*Itr).TelescopeDirection * ApparentTelescopeDirectionVector).Length(),
				 (*Itr).TelescopeDirection ^ ApparentTelescopeDirectionVector)] = &(*Itr);	
                    }
                    // compute the transform
                    std::map<double, const AlignmentDatabaseEntry *>::const_iterator Nearest = NearestMap.begin();
                    const AlignmentDatabaseEntry *pEntry1                                    = (*Nearest).second;
		    ln_equ_posn DummyRaDec;
		    ln_equ_posn ActualSyncPoint1;		    
		    TelescopeDirectionVector ActualDirectionCosine1;
		    ln_equ_posn RaDec1;
		    RaDec1.dec = pEntry1->Declination;
                    // libnova works in decimal degrees so conversion is needed here
                    RaDec1.ra  = pEntry1->RightAscension * 360.0 / 24.0;
		    double lstdegrees1 = (ln_get_apparent_sidereal_time(pEntry1->ObservationJulianDate) * 360.0 / 24.0) + Position.lng;
		    ActualSyncPoint1.ra = (lstdegrees1 - RaDec1.ra);
		    ActualSyncPoint1.dec = RaDec1.dec;
		    ActualDirectionCosine1 =  TelescopeDirectionVectorFromLocalHourAngleDeclination(ActualSyncPoint1);
		    TelescopeDirectionVector DummyActualDirectionCosine2;
		    TelescopeDirectionVector DummyApparentDirectionCosine2;
		    TelescopeDirectionVector DummyActualDirectionCosine3;
		    TelescopeDirectionVector DummyApparentDirectionCosine3;
		    gsl_matrix *R =          gsl_matrix_alloc(3,3);
		    gsl_vector *pActual1 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent1 = gsl_vector_alloc(3);
		    gsl_vector *pActual2 =   gsl_vector_alloc(3);
		    gsl_vector *pApparent2 = gsl_vector_alloc(3);
		    DummyRaDec.ra  = range24(pEntry1->RightAscension + 6.0);
                    DummyRaDec.dec = pEntry1->Declination;
		    DummyActualDirectionCosine2   = TelescopeDirectionVectorFromLocalHourAngleDeclination(DummyRaDec);
		    // geehalel: computes the rotation matrix from Actual1 to Apparent1
		    gsl_vector_set(pActual1, 0, ActualDirectionCosine1.x);
		    gsl_vector_set(pActual1, 1, ActualDirectionCosine1.y);
		    gsl_vector_set(pActual1, 2, ActualDirectionCosine1.z);
		    gsl_vector_set(pApparent1, 0, pEntry1->TelescopeDirection.x);
		    gsl_vector_set(pApparent1, 1, pEntry1->TelescopeDirection.y);
		    gsl_vector_set(pApparent1, 2, pEntry1->TelescopeDirection.z);
		    RotationMatrixFromVectors(pActual1, pApparent1, R);
		    Dump3x3("Rot",R);
		    // geehale: and applies the rotation to Actual2 (Dummy point above)
		    gsl_vector_set(pActual2, 0, DummyActualDirectionCosine2.x);
		    gsl_vector_set(pActual2, 1, DummyActualDirectionCosine2.y);
		    gsl_vector_set(pActual2, 2, DummyActualDirectionCosine2.z);
		    MatrixVectorMultiply(R, pActual2, pApparent2);
		    DummyApparentDirectionCosine2.x = gsl_vector_get(pApparent2, 0);
		    DummyApparentDirectionCosine2.y = gsl_vector_get(pApparent2, 1);
		    DummyApparentDirectionCosine2.z = gsl_vector_get(pApparent2, 2);
		    DummyActualDirectionCosine3 = ActualDirectionCosine1 * DummyActualDirectionCosine2;
		    DummyActualDirectionCosine3.Normalise();
		    DummyApparentDirectionCosine3 = pEntry1->TelescopeDirection * DummyApparentDirectionCosine2;
		    DummyApparentDirectionCosine3.Normalise();
		    pComputedTransform = gsl_matrix_alloc(3, 3);
		    gsl_matrix *pUnused = gsl_matrix_alloc(3,3);
		    CalculateTransformMatrices(ActualDirectionCosine1, DummyActualDirectionCosine2, DummyActualDirectionCosine3,
					       pEntry1->TelescopeDirection, DummyApparentDirectionCosine2,
					       DummyApparentDirectionCosine3, pUnused,
					       pComputedTransform);
		    gsl_matrix_free(pUnused);
		    // geehalel Nearest
                    pTransform = pComputedTransform;
                }
                else
                    pTransform = CurrentFace->pMatrix;
            }
            else
                return false;

            // OK - got an intersection - CurrentFace is pointing at the face
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLApparentVector, 0, ApparentTelescopeDirectionVector.x);
            gsl_vector_set(pGSLApparentVector, 1, ApparentTelescopeDirectionVector.y);
            gsl_vector_set(pGSLApparentVector, 2, ApparentTelescopeDirectionVector.z);
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            MatrixVectorMultiply(pTransform, pGSLApparentVector, pGSLActualVector);
            TelescopeDirectionVector ActualTelescopeDirectionVector;
            ActualTelescopeDirectionVector.x = gsl_vector_get(pGSLActualVector, 0);
            ActualTelescopeDirectionVector.y = gsl_vector_get(pGSLActualVector, 1);
            ActualTelescopeDirectionVector.z = gsl_vector_get(pGSLActualVector, 2);
            ActualTelescopeDirectionVector.Normalise();
	    
	    switch (ApproximateMountAlignment)
            {
                case ZENITH:
		{
		    ln_hrz_posn ActualAltAz;
		    AltitudeAzimuthFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualAltAz);
		    ln_get_equ_from_hrz(&ActualAltAz, &Position, ln_get_julian_from_sys(), &ActualRaDec);
		    break;
		}
	        case NORTH_CELESTIAL_POLE:
                case SOUTH_CELESTIAL_POLE:
		{  
		    ln_equ_posn ActualHourAngleDec;
		    double lstdegrees = (ln_get_apparent_sidereal_time(ln_get_julian_from_sys()) * 360.0 / 24.0) + Position.lng;
		    LocalHourAngleDeclinationFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualHourAngleDec);
		    ActualRaDec.ra= (lstdegrees - ActualHourAngleDec.ra);
		    ActualRaDec.dec =  ActualHourAngleDec.dec;
                    break;
		}
	    }	    
	    // libnova works in decimal degrees so conversion is needed here
            RightAscension = ActualRaDec.ra * 24.0 / 360.0;
            Declination    = ActualRaDec.dec;
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            if (nullptr != pComputedTransform)
                gsl_matrix_free(pComputedTransform);
            break;
        }
    }
    //ASSDEBUGF("Telescope to Celestial - Actual Alt %lf Az %lf", ActualAltAz.alt, ActualAltAz.az);
    RightAscension = range24(RightAscension);
    return true;
}

// Private methods

void BasicMathPlugin::Dump3(const char *Label, gsl_vector *pVector)
{
    ASSDEBUGF("Vector dump - %s", Label);
    ASSDEBUGF("%lf %lf %lf", gsl_vector_get(pVector, 0), gsl_vector_get(pVector, 1), gsl_vector_get(pVector, 2));
}

void BasicMathPlugin::Dump3x3(const char *Label, gsl_matrix *pMatrix)
{
    ASSDEBUGF("Matrix dump - %s", Label);
    ASSDEBUGF("Row 0 %lf %lf %lf", gsl_matrix_get(pMatrix, 0, 0), gsl_matrix_get(pMatrix, 0, 1),
              gsl_matrix_get(pMatrix, 0, 2));
    ASSDEBUGF("Row 1 %lf %lf %lf", gsl_matrix_get(pMatrix, 1, 0), gsl_matrix_get(pMatrix, 1, 1),
              gsl_matrix_get(pMatrix, 1, 2));
    ASSDEBUGF("Row 2 %lf %lf %lf", gsl_matrix_get(pMatrix, 2, 0), gsl_matrix_get(pMatrix, 2, 1),
              gsl_matrix_get(pMatrix, 2, 2));
}

/// Use gsl to compute the determinant of a 3x3 matrix
double BasicMathPlugin::Matrix3x3Determinant(gsl_matrix *pMatrix)
{
    gsl_permutation *pPermutation = gsl_permutation_alloc(3);
    gsl_matrix *pDecomp           = gsl_matrix_alloc(3, 3);
    int Signum;
    double Determinant;

    gsl_matrix_memcpy(pDecomp, pMatrix);

    gsl_linalg_LU_decomp(pDecomp, pPermutation, &Signum);

    Determinant = gsl_linalg_LU_det(pDecomp, Signum);

    gsl_matrix_free(pDecomp);
    gsl_permutation_free(pPermutation);

    return Determinant;
}

/// Use gsl to compute the inverse of a 3x3 matrix
bool BasicMathPlugin::MatrixInvert3x3(gsl_matrix *pInput, gsl_matrix *pInversion)
{
    bool Retcode                  = true;
    gsl_permutation *pPermutation = gsl_permutation_alloc(3);
    gsl_matrix *pDecomp           = gsl_matrix_alloc(3, 3);
    int Signum;

    gsl_matrix_memcpy(pDecomp, pInput);

    gsl_linalg_LU_decomp(pDecomp, pPermutation, &Signum);

    // Test for singularity
    if (0 == gsl_linalg_LU_det(pDecomp, Signum))
    {
        Retcode = false;
    }
    else
        gsl_linalg_LU_invert(pDecomp, pPermutation, pInversion);

    gsl_matrix_free(pDecomp);
    gsl_permutation_free(pPermutation);

    return Retcode;
}

/// Use gsl blas support to multiply two matrices together and put the result in a third.
/// For our purposes all the matrices should be 3 by 3.
void BasicMathPlugin::MatrixMatrixMultiply(gsl_matrix *pA, gsl_matrix *pB, gsl_matrix *pC)
{
    // Zeroise the output matrix
    gsl_matrix_set_zero(pC);

    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, pA, pB, 0.0, pC);
}

/// Use gsl blas support to multiply a matrix by a vector and put the result in another vector
/// For our purposes the the matrix should be 3x3 and vector 3.
void BasicMathPlugin::MatrixVectorMultiply(gsl_matrix *pA, gsl_vector *pB, gsl_vector *pC)
{
    // Zeroise the output vector
    gsl_vector_set_zero(pC);

    gsl_blas_dgemv(CblasNoTrans, 1.0, pA, pB, 0.0, pC);
}

bool BasicMathPlugin::RayTriangleIntersection(TelescopeDirectionVector &Ray, TelescopeDirectionVector &TriangleVertex1,
                                              TelescopeDirectionVector &TriangleVertex2,
                                              TelescopeDirectionVector &TriangleVertex3)
{
    // Use Möller-Trumbore

    //Find vectors for two edges sharing V1
    TelescopeDirectionVector Edge1 = TriangleVertex2 - TriangleVertex1;
    TelescopeDirectionVector Edge2 = TriangleVertex3 - TriangleVertex1;

    TelescopeDirectionVector P = Ray * Edge2; // cross product
    double Determinant         = Edge1 ^ P;   // dot product
    double InverseDeterminant  = 1.0 / Determinant;

    // If the determinant is negative the triangle is backfacing
    // If the determinant is close to 0, the ray misses the triangle
    if ((Determinant > -std::numeric_limits<double>::epsilon()) &&
        (Determinant < std::numeric_limits<double>::epsilon()))
        return false;

    // I use zero as ray origin so
    TelescopeDirectionVector T(-TriangleVertex1.x, -TriangleVertex1.y, -TriangleVertex1.z);

    // Calculate the u parameter
    double u = (T ^ P) * InverseDeterminant;

    if (u < 0.0 || u > 1.0)
        //The intersection lies outside of the triangle
        return false;

    //Prepare to test v parameter
    TelescopeDirectionVector Q = T * Edge1;

    //Calculate v parameter and test bound
    double v = (Ray ^ Q) * InverseDeterminant;

    if (v < 0.0 || u + v > 1.0)
        //The intersection lies outside of the triangle
        return false;

    double t = (Edge2 ^ Q) * InverseDeterminant;

    if (t > std::numeric_limits<double>::epsilon())
    {
        //ray intersection
        return true;
    }

    // No hit, no win
    return false;
}

void BasicMathPlugin::CrossProduct(const gsl_vector *u, const gsl_vector *v, gsl_vector *product)
{
        double p1 = gsl_vector_get(u, 1)*gsl_vector_get(v, 2)
                - gsl_vector_get(u, 2)*gsl_vector_get(v, 1);

        double p2 = gsl_vector_get(u, 2)*gsl_vector_get(v, 0)
                - gsl_vector_get(u, 0)*gsl_vector_get(v, 2);

        double p3 = gsl_vector_get(u, 0)*gsl_vector_get(v, 1)
                - gsl_vector_get(u, 1)*gsl_vector_get(v, 0);

        gsl_vector_set(product, 0, p1);
        gsl_vector_set(product, 1, p2);
        gsl_vector_set(product, 2, p3);
}

/// Use gsl blas support to compute the rotation matrix pR that rotates unit vector pA to unit vector pB
/// For our purposes the the matrix should be 3x3 and vectors 3.
/// see https://math.stackexchange.com/questions/180418/calculate-rotation-matrix-to-align-vector-a-to-vector-b-in-3d
/// geehalel
void BasicMathPlugin::RotationMatrixFromVectors(gsl_vector *pA, gsl_vector *pB, gsl_matrix *pR)
{
    gsl_vector *v   = gsl_vector_alloc(3);
    gsl_matrix *vx  = gsl_matrix_alloc(3, 3);
    gsl_matrix *vx2 = gsl_matrix_alloc(3, 3);
    double s, c; // sine and cos
    // v = A x B
    CrossProduct(pA, pB, v);

    // s = norm(v)
    s = gsl_blas_dnrm2(v);

    // c = A . B
    gsl_blas_ddot(pA, pB, &c);

    // initialise vx
    gsl_matrix_set(vx, 0, 0, 0.0);
    gsl_matrix_set(vx, 0, 1, -gsl_vector_get(v, 2));
    gsl_matrix_set(vx, 0, 2, gsl_vector_get(v, 1));
    gsl_matrix_set(vx, 1, 0, gsl_vector_get(v, 2));
    gsl_matrix_set(vx, 1, 1, 0.0);
    gsl_matrix_set(vx, 1, 2, -gsl_vector_get(v, 0));
    gsl_matrix_set(vx, 2, 0, -gsl_vector_get(v, 1));
    gsl_matrix_set(vx, 2, 1, gsl_vector_get(v, 0));
    gsl_matrix_set(vx, 2, 2, 0.0);

    // compute vx2
    MatrixMatrixMultiply(vx, vx, vx2);

    // R = I + vx + (1-c)/s^2 vx2
    gsl_matrix_set_identity(pR);
    gsl_matrix_add(pR, vx);
    gsl_matrix_scale(vx2, (1.0 -c) / (s * s));
    gsl_matrix_add(pR, vx2);
    

    gsl_vector_free(v);
    gsl_matrix_free(vx);
    gsl_matrix_free(vx2);
}

std::string BasicMathPlugin::GetInternalDataRepresentation(std::string PluginDisplayName)
{
    std::string repr = MathPlugin::GetInternalDataRepresentation(PluginDisplayName);
    size_t InsertPosition = repr.find_last_of('\n');
    std::string tail = repr.substr(InsertPosition + 1);
    repr = repr.substr(0, InsertPosition + 1);
    repr += "<InternalData>\n";
    ConvexHull::tFace CurrentFace = ActualConvexHull.faces;
    if (nullptr != CurrentFace)
    {
        char buf[32];
	std::string strbuf;
        repr += "  <ActualConvexHullFaces>\n";
        do
	{
	    if ((0 != CurrentFace->vertex[0]->vnum) && (0 != CurrentFace->vertex[1]->vnum) &&
		(0 != CurrentFace->vertex[2]->vnum))
	    {
	        repr += "    <Face>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[0]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[1]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[2]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		repr += "      <Matrix>\n";
		for (int row = 0; row < 3; row++)
		{
		    std::snprintf(buf, 32, "%d", row);
		    strbuf = buf;
		    repr += "        <Row id='" + strbuf + "'>";
		    for (int col = 0; col < 3; col++)
		    {
		        std::snprintf(buf, 32, "%lf", gsl_matrix_get(CurrentFace->pMatrix, row, col));
			strbuf = buf;
			repr += "<Cell>" + strbuf + "</Cell>";
		    }
		    repr += "</Row>\n";
		}
		repr += "      </Matrix>\n";  
		repr += "    </Face>\n"; 
	    }
	    CurrentFace = CurrentFace->next;
	}  while (CurrentFace != ActualConvexHull.faces);
	repr += "  </ActualConvexHullFaces>\n";
    }
    CurrentFace = ApparentConvexHull.faces;
    if (nullptr != CurrentFace)
    {
        char buf[32];
	std::string strbuf;
        repr += "  <ApparentConvexHullFaces>\n";
        do
	{
	    if ((0 != CurrentFace->vertex[0]->vnum) && (0 != CurrentFace->vertex[1]->vnum) &&
		(0 != CurrentFace->vertex[2]->vnum))
	    {
	        repr += "    <Face>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[0]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[1]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		std::snprintf(buf, 32, "%d", CurrentFace->vertex[2]->vnum);
		strbuf = buf;
		repr += "      <Vertex>" + strbuf + "</Vertex>\n";
		repr += "      <Matrix>\n";
		for (int row = 0; row < 3; row++)
		{
		    std::snprintf(buf, 32, "%d", row);
		    strbuf = buf;
		    repr += "        <Row id='" + strbuf + "'>";
		    for (int col = 0; col < 3; col++)
		    {
		        std::snprintf(buf, 32, "%lf", gsl_matrix_get(CurrentFace->pMatrix, row, col));
			strbuf = buf;
			repr += "<Cell>" + strbuf + "</Cell>";
		    }
		    repr += "</Row>\n";
		}
		repr += "      </Matrix>\n";  
		repr += "    </Face>\n"; 
	    }
	    CurrentFace = CurrentFace->next;
	}  while (CurrentFace != ApparentConvexHull.faces);
	repr += "  </ApparentConvexHullFaces>\n";
    }
    repr += "</InternalData>\n";
    return repr + tail;
}
  
} // namespace AlignmentSubsystem
} // namespace INDI
