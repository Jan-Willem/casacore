//# Coordinate.cc: this defines the Coordinate class
//# Copyright (C) 1997,1998,1999,2000,2001,2002,2003
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This library is free software; you can redistribute it and/or modify it
//# under the terms of the GNU Library General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or (at your
//# option) any later version.
//#
//# This library is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
//# License for more details.
//#
//# You should have received a copy of the GNU Library General Public License
//# along with this library; if not, write to the Free Software Foundation,
//# Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA
//#
//#
//# $Id$

#include <trial/Coordinates/Coordinate.h>

#include <aips/Arrays/Matrix.h>
#include <aips/Arrays/Vector.h>
#include <aips/Arrays/ArrayMath.h>
#include <aips/Arrays/ArrayLogical.h>
#include <aips/Arrays/ArrayAccessor.h>
#include <trial/Coordinates/Projection.h>
#include <aips/Exceptions/Error.h>
#include <aips/Logging/LogIO.h>
#include <aips/Mathematics/Math.h>
#include <aips/Measures/MDirection.h>
#include <aips/Quanta/Unit.h>
#include <aips/Utilities/Assert.h>
#include <aips/Utilities/Regex.h>

#include <aips/OS/Timer.h>


#include <aips/iomanip.h>  
#include <aips/sstream.h>


Coordinate::Coordinate()
{}


Coordinate::Coordinate(const Coordinate& other)
: error_p(other.error_p)
{}

Coordinate& Coordinate::operator=(const Coordinate& other)
{
   if (this != &other) {
      error_p = other.error_p;
   }
   return *this;
}
 

Coordinate::~Coordinate()
{}


uInt Coordinate::toWorldMany(Matrix<Double>& world, 
			     const Matrix<Double>& pixel, 
			     Vector<Int>& failures) const
{
    AlwaysAssert(nPixelAxes()==pixel.nrow(), AipsError);
    const uInt nTransforms = pixel.ncolumn();
    world.resize(nWorldAxes(), nTransforms);
//
    Vector<Double> pixTmp(nPixelAxes());
    Vector<Double> lastPix(nPixelAxes());
    Vector<Double> worldTmp(nWorldAxes());
//
    ArrayAccessor<Double, Axis<1> > jPixel(pixel);
    ArrayAccessor<Double, Axis<1> > jWorld(world);
//
    String errorMsg;
    uInt nError = 0;
    uInt k,l;
    Bool same;
    ArrayAccessor<Double, Axis<0> > iPixel, iWorld;
//
    for (jPixel.reset(),jWorld.reset(),l=0; jPixel!=jPixel.end(); ++jPixel,++jWorld,l++) {
       iPixel = jPixel;            // Partial assignment
       same = True;
       for (iPixel.reset(),k=0; iPixel!=iPixel.end(); ++iPixel,k++) {
          pixTmp[k] = *iPixel;
          if (l==0 || (l!=0 && !::near(pixTmp[k],lastPix[k]))) same = False;
       }
//
       iWorld = jWorld;            // Partial assigment
       if (same) {
          for (iWorld.reset(),k=0; iWorld!=iWorld.end(); ++iWorld,k++) {
             *iWorld = worldTmp[k];         // Copy last conversion
          }
       } else {
          if (!toWorld (worldTmp, pixTmp)) {
             nError++;
             if (nError > failures.nelements()) {
                failures.resize(2*nError, True);
             }
             failures(nError-1) = l;
             if (nError == 1) errorMsg = errorMessage();    // Save the first error message
           } else {
              for (iWorld.reset(),k=0; iWorld!=iWorld.end(); ++iWorld,k++) {
                 *iWorld = worldTmp[k];
              }
           }
        }
//
        lastPix = pixTmp;
    }
//
    if (nError != 0) set_error(errorMsg); // put back the first error
    return nError;

}



uInt Coordinate::toPixelMany(Matrix<Double>& pixel, 
			     const Matrix<Double>& world,
			     Vector<Int>& failures) const
{
    AlwaysAssert(nWorldAxes()==world.nrow(), AipsError);
    const uInt nTransforms = world.ncolumn();
    pixel.resize(nPixelAxes(), nTransforms);
//
    Vector<Double> pixTmp(nPixelAxes());
    Vector<Double> worldTmp(nWorldAxes());
    Vector<Double> lastWorld(nWorldAxes());
//
    ArrayAccessor<Double, Axis<1> > jPixel(pixel);
    ArrayAccessor<Double, Axis<1> > jWorld(world);
//
    String errorMsg;
    uInt nError = 0;
    uInt k,l;
    Bool same;
    ArrayAccessor<Double, Axis<0> > iPixel, iWorld;
//
    for (jWorld.reset(),jPixel.reset(),l=0; jWorld!=jWorld.end(); ++jWorld,++jPixel,l++) {
       iWorld = jWorld;           // Partial assigment
       same = True;
       for (iWorld.reset(),k=0; iWorld!=iWorld.end(); ++iWorld,k++) {
          worldTmp[k] = *iWorld;
          if (l==0 || (l!=0 && !::near(worldTmp[k],lastWorld[k]))) same = False;
       }
//
       iPixel = jPixel;          // Partial assignment
       if (same) {
          for (iPixel.reset(),k=0; iPixel!=iPixel.end(); ++iPixel,k++) {
             *iPixel= pixTmp[k];       // Copy last conversion
           }
       } else {
          if (!toPixel(pixTmp, worldTmp)) {
             nError++;
             if (nError > failures.nelements()) {
                failures.resize(2*nError, True);
             }
             failures(nError-1) = l;
             if (nError == 1) errorMsg = errorMessage();    // Save the first error message
           } else {
              for (iPixel.reset(),k=0; iPixel!=iPixel.end(); ++iPixel,k++) {
                 *iPixel= pixTmp[k];
              }
           }
        }
//
        lastWorld = worldTmp;
    }
//
    if (nError != 0) set_error(errorMsg); // put back the first error
    return nError;
}



Bool Coordinate::toMix(Vector<Double>& worldOut,
                       Vector<Double>& pixelOut,
                       const Vector<Double>& worldIn,
                       const Vector<Double>& pixelIn,
                       const Vector<Bool>& worldAxes,   
                       const Vector<Bool>& pixelAxes,
                       const Vector<Double>&,
                       const Vector<Double>&) const
//
// Default implementation ok for non-coupled coordinated like
// Linear.  Coupled coordinates like DirectionCoordinate
// need their own implementation
//
{
    static Vector<Double> pixel_tmp;
    static Vector<Double> world_tmp;

   const uInt nWorld = worldAxes.nelements();
   const uInt nPixel = pixelAxes.nelements();
//
   DebugAssert(nWorld == nWorldAxes(), AipsError);
   DebugAssert(worldIn.nelements()==nWorld, AipsError);
   DebugAssert(nPixel == nPixelAxes(), AipsError);   
   DebugAssert(pixelIn.nelements()==nPixel, AipsError);   
//
   for (uInt i=0; i<nPixel; i++) {
      if (pixelAxes(i) && worldAxes(i)) {
         set_error("Coordinate::toMix - duplicate pixel/world axes");
         return False;
      }
      if (!pixelAxes(i) && !worldAxes(i)) {
         set_error("Coordinate::toMix - each axis must be either pixel or world");
         return False;
      }
   }
//
// Resize happens first time or maybe after an assignment
//
   if (world_tmp.nelements()!=nWorld) world_tmp.resize(nWorld);
   if (pixel_tmp.nelements()!=nPixel) pixel_tmp.resize(nPixel);
//
// Convert world to pixel.  Use  reference value unless
// world value given. Copy output pixels to output vector 
// and overwrite with any input pixel values that were given
//
   world_tmp = referenceValue();
   for (uInt i=0; i<nWorld; i++) {
      if (worldAxes(i)) world_tmp(i) = worldIn(i);
   }
   if (!toPixel(pixel_tmp,world_tmp)) return False;
//
   if (pixelOut.nelements()!=nPixel) pixelOut.resize(nPixel);
   pixelOut = pixel_tmp;
   for (uInt i=0; i<nPixel; i++) {
      if (pixelAxes(i)) pixelOut(i) = pixelIn(i);
   }
//
// Convert pixel to world.  Use reference pixel unless
// pixel value given. Copy output worlds to output vector 
// and overwrite with any input world values that were given
//
   pixel_tmp = referencePixel();
   for (uInt i=0; i<nPixel; i++) {
      if (pixelAxes(i)) pixel_tmp(i) = pixelIn(i);
   }
   if (!toWorld(world_tmp,pixel_tmp)) return False;
   if (worldOut.nelements()!=nWorld) worldOut.resize(nWorld);
   worldOut = world_tmp;
   for (uInt i=0; i<nWorld; i++) {
      if (worldAxes(i)) worldOut(i) = worldIn(i);
   }
//
   return True;
}


// Does everything except set the units vector, which must be done in the derived class.
Bool Coordinate::setWorldAxisUnits(const Vector<String> &units)
{
    if (units.nelements() != nWorldAxes()) {
	set_error("Wrong number of elements in units vector");
	return False;
    } else {
	// If the units are unchanged just return True.
	Vector<String> old = worldAxisUnits();
	if (allEQ(old, units)) {
	    return True;
	}
    }

    Bool ok = True;

    String error;
    Vector<Double> factor;
    ok = find_scale_factor(error, factor, units, worldAxisUnits());
    if (ok) {
      ok = setIncrement(increment() * factor);
      if (ok) {
         ok = setReferenceValue(referenceValue() * factor);
      }
    } else {
      set_error(error);
    }

    return ok;
}

void Coordinate::checkFormat(Coordinate::formatType& format, 
                             const Bool ) const
{
// Scientific or fixed formats only are allowed.
// Absolute or offset is irrelevant
 
   if (format == Coordinate::DEFAULT) {
      format = Coordinate::SCIENTIFIC;
   } else {
      if (format != Coordinate::SCIENTIFIC &&
          format != Coordinate::FIXED) format = Coordinate::SCIENTIFIC;
   }
}


void Coordinate::getPrecision(Int &precision,
                              Coordinate::formatType& format,
                              Bool absolute,
                              Int defPrecScientific,
                              Int defPrecFixed,
                              Int ) const
{
// Scientific or fixed formats only are allowed.
// Absolute or offset is irrelevant
 
   checkFormat (format, absolute);
   
   if (format == Coordinate::SCIENTIFIC) {
      if (defPrecScientific >= 0) {
         precision = defPrecScientific;
      } else {
         precision = 6;              
      }
   } else if (format == Coordinate::FIXED) {
      if (defPrecFixed >= 0) {
         precision = defPrecFixed;
      } else {
         precision = 6;
      }
   }
}


String Coordinate::format(String& units,
                          Coordinate::formatType format, 
                          Double worldValue, 
                          uInt worldAxis, 
                          Bool isAbsolute, 
                          Bool showAsAbsolute,
                          Int precision)
//
// isAbsolute
//    T means the worldValue is given as absolute
//    F means the worldValue is given as relative
// 
// showAsAbsolute
//    T means the worldValue should be formatted as absolute
//    F means the worldValue should be formatted as relative
//
{
   DebugAssert(worldAxis < nWorldAxes(), AipsError);
 
// Check format

   Coordinate::formatType form = format;
   checkFormat (form, showAsAbsolute);
   
// Set default precision
 
   Int prec = precision;
   if (prec < 0) getPrecision(prec, form, showAsAbsolute, -1, -1, -1);

// Convert given world value to absolute or relative as needed

   static Vector<Double> world;   
   if (world.nelements()!=nWorldAxes()) world.resize(nWorldAxes());
//
   if (showAsAbsolute) {
      if (!isAbsolute) {
         world = 0.0;
         world(worldAxis) = worldValue;
         makeWorldAbsolute(world);
         worldValue = world(worldAxis); 
      }
   } else {
      if (isAbsolute) {
         world = referenceValue();
         world(worldAxis) = worldValue;
         makeWorldRelative(world);
         worldValue = world(worldAxis); 
      }
   } 


// If units are empty used preferred unit.
// Convert to specified unit if possible

   String nativeUnit = worldAxisUnits()(worldAxis);
   if (units.empty()) {
      String prefUnit = preferredWorldAxisUnits()(worldAxis);
      if (prefUnit.empty()) {
         units = nativeUnit;
      } else {
         units = prefUnit;
      }
   }

// Now check validity of unit

   Unit nativeUnitU(nativeUnit);
   Unit currentUnitU(units);
//
   if (currentUnitU != nativeUnitU) {
      throw(AipsError("Requested units are invalid for this Coordinate"));
   } else {
      static Quantum<Double> q;
      q.setValue(worldValue);
      q.setUnit(nativeUnitU);
      worldValue = q.getValue(currentUnitU);
   }
       
// Format and get units.  
         
   ostringstream oss;
   if (form == Coordinate::SCIENTIFIC) {
      oss.setf(ios::scientific, ios::floatfield);
      oss.precision(prec);
      oss << worldValue;
   } else if (form == Coordinate::FIXED) {
      oss.setf(ios::fixed, ios::floatfield);
      oss.precision(prec);
      oss << worldValue;        
   }                            
//
   return String(oss);
}


String Coordinate::formatQuantity (String& units,
                                   Coordinate::formatType format2, 
                                   const Quantum<Double>& worldValue, 
                                   uInt worldAxis, 
                                   Bool isAbsolute,
                                   Bool showAsAbsolute,
                                   Int precision)
{
   DebugAssert(worldAxis < nWorldAxes(), AipsError);

// Use derived class formatter

   return format(units, format2, 
                 worldValue.getValue(Unit(worldAxisUnits()(worldAxis))),
                 worldAxis, isAbsolute, showAsAbsolute, precision);
}


// after = factor * before
Bool Coordinate::find_scale_factor(String &error, Vector<Double> &factor, 
				   const Vector<String> &units, 
				   const Vector<String> &oldUnits)
{
    factor.resize(units.nelements());
    Bool ok = (units.nelements() == oldUnits.nelements());
    if (! ok) {
	error = "units and oldUnits are different sizes!";
    } else {
	// Try to find the scaling factors between the old and new units
	uInt n = units.nelements();
	for (uInt i=0; i<n && ok; i++) {
	    if (UnitVal::check(oldUnits(i)) && UnitVal::check(units(i))) {
		Unit before = oldUnits(i);
		Unit after = units(i);
		ok = (before.getValue() == after.getValue());
		if (!ok) {
		    error = "Units are not compatible dimensionally";
		} else {
		    factor(i) = before.getValue().getFac() / 
			after.getValue().getFac();
		}
	    } else {
		ok = False;
		error = "Unknown unit - cannot calculate scaling";
	    }
	}
    }
    return ok;
}


String Coordinate::typeToString (Coordinate::Type type)
{
// 
// I would prefer to call the virtual function
// Coordinate::showType() but then I need an object
//
   if (type==Coordinate::LINEAR) {
      return String("Linear");
   } else if (type==Coordinate::DIRECTION) {
      return String("Direction");
   } else if (type==Coordinate::SPECTRAL) {
      return String("Spectral");
   } else if (type==Coordinate::STOKES) {
      return String("Stokes");
   } else if (type==Coordinate::TABULAR) {
      return String("Tabular");
   } else if (type==Coordinate::COORDSYS) {      
      return String("System");
   } else {
      return String("Unknown - function Coordinate::typeToString needs development");
   }
   return String("");
}

void Coordinate::set_error(const String &errorMsg) const
{
    error_p = errorMsg;
}




Vector<String> Coordinate::make_Direction_FITS_ctype (Bool& isNCP, const Projection& proj,
                                                      const Vector<String>& axisNames,
                                                      Double refLat, Bool printError) const
//
// Reflat in radians
//
{
    LogIO os(LogOrigin("Coordinate", "make_Direction_FITS_ctype", WHERE));
    Vector<String> ctype(2);
    Vector<Double> projParameters = proj.parameters();
//
    isNCP = False;
    for (uInt i=0; i<2; i++) {
       String name = axisNames(i);
       while (name.length() < 4) {
           name += "-";
       }
       switch(proj.type()) {
          case Projection::TAN:  // Fallthrough
          case Projection::ARC:
              name = name + "-" + proj.name();
              break;
          case Projection::SIN:

// This is either "real" SIN or NCP

              AlwaysAssert(projParameters.nelements() == 2, AipsError);
              if (::near(projParameters(0), 0.0) && ::near(projParameters(1), 0.0)) {
                  // True SIN
                  name = name + "-" + proj.name();
              } else {

// NCP?  From Greisen and Calabretta
// The potential divide by zero should never occur in
// a real DirectionCoordinate as you better not have observed at
// lat=0 with an EW array

                  if (::near(projParameters(0), 0.0) &&
                      ::near(projParameters(1), 1.0/tan(refLat))) {
                      isNCP = True;
                      name = name + "-NCP";
                  } else {

// Doesn't appear to be NCP
// Only print this once rather than twice

                      if (!isNCP) {
                          os << LogIO::WARN << "SIN projection with non-zero"
                              " projp does not appear to be NCP." << endl <<
                              "However, assuming NCP anyway." << LogIO::POST;

                      }
                      name = name + "-NCP";
                      isNCP = True;
                  }
              }
              break;
          default:
             if (i == 0) {

// Only print the message once for long/lat

                if (printError) {
                   os << LogIO::WARN << proj.name()
                      << " is not known to standard FITS (it is known to WCS)."
                      << LogIO::POST;
                }
             }
             name = name + "-" + proj.name();
             break;
       }
       ctype(i) = name;
    }
    return ctype;
}


Coordinate* Coordinate::makeFourierCoordinate (const Vector<Bool>& axes,
                                               const Vector<Int>& shape)  const
{
   String tmp = String("Coordinates of type ") + showType() + String(" cannot be Fourier Transformed");
   throw(AipsError(tmp));
}


void Coordinate::fourierUnits (String& nameOut, String& unitOut, String& unitInCanon,
                               Coordinate::Type type, Int axis,   
                               const String& unitIn,
                               const String& nameIn) const

//
// A disgusting fudgy routine to work out some nice names and units
// Fourier coordinates.  Rather limited in its knowledge currently.
//
{
   Unit time("s");
   Unit freq("Hz");
   Unit rad("rad");
   Unit unitIn2(unitIn);
//
   if (type==Coordinate::DIRECTION) {
      if (unitIn2==rad) {
         unitInCanon = String("rad");
         if (axis==0) {
            nameOut = String("UU");
         } else if (axis==1) {
            nameOut = String("VV");
         } else {
            throw(AipsError("Illegal DirectionCoordinate axis"));
         }
         unitOut = String("lambda");
      } else {
         nameOut = String("Inverse(") + nameIn + String(")");
         unitOut = String("1/") + unitIn;
         unitInCanon = unitIn;
      }
   } else if (type==Coordinate::LINEAR ||
              type==Coordinate::SPECTRAL ||
              type==Coordinate::TABULAR) {
      if (unitIn2==freq) {
         nameOut = String("Time");
         unitOut = String("s");
         unitInCanon = "Hz";
      } else if (unitIn2==time) {
         nameOut = String("Frequency");
         unitOut = String("Hz");
         unitInCanon = "s";
      } else {
         nameOut = String("Inverse(") + nameIn + String(")");
         unitOut = String("1/") + unitIn;
         unitInCanon = unitIn;
      }
   } else if (type==Coordinate::STOKES) {
      throw (AipsError("Cannot provide Fourier coordinate name for Stokes coordinate"));
   } else if (type==Coordinate::COORDSYS) {
      throw (AipsError("Cannot provide Fourier coordinate name for CoordinateSystem coordinate"));
   } else {
      nameOut = String("Inverse(") + nameIn + String(")");
      unitOut = String("1/") + unitIn;
      unitInCanon = unitIn;
   }
}


void Coordinate::makeWorldAbsoluteMany (Matrix<Double>& value) const
{
   makeWorldAbsRelMany (value, True);
}

void Coordinate::makeWorldRelativeMany (Matrix<Double>& value) const
{
   makeWorldAbsRelMany (value, False);
}

void Coordinate::makePixelAbsoluteMany (Matrix<Double>& value) const
{
   makePixelAbsRelMany (value, True);
}

void Coordinate::makePixelRelativeMany (Matrix<Double>& value) const
{
   makePixelAbsRelMany (value, False);
}


void Coordinate::makeWorldAbsRelMany (Matrix<Double>& value, Bool toAbs) const
{
    Vector<Double> col(nWorldAxes());
    Vector<Double> lastInCol(nWorldAxes());    
    Vector<Double> lastOutCol(nWorldAxes());
    uInt k,l;
    Bool same;
    ArrayAccessor<Double, Axis<0> > i;
    ArrayAccessor<Double, Axis<1> > j(value);
    for (j.reset(),l=0; j!=j.end(); j++,l++) {
       i = j;
       same = True;
       for (i.reset(),k=0; i!=i.end(); i++,k++) {
          col[k] = *i;
          if (l==0 || (l!=0 && !::near(col[k],lastInCol[k]))) same = False;
       }
       lastInCol = col;
//
       if (same) {
          for (i.reset(),k=0; i!=i.end(); ++i,k++) {
             *i = lastOutCol[k];
          }
       } else {
          if (toAbs) {
             makeWorldAbsolute(col);
          } else {
             makeWorldRelative(col);
          }
//
          for (i.reset(),k=0; i!=i.end(); ++i,k++) {
             *i = col[k];
          }
          lastOutCol = col;
       }
    }
}



void Coordinate::makePixelAbsRelMany (Matrix<Double>& value, Bool abs) const
{
    Vector<Double> col(nPixelAxes());
    Vector<Double> lastInCol(nPixelAxes());    
    Vector<Double> lastOutCol(nPixelAxes());
    uInt k,l;
    Bool same;
    ArrayAccessor<Double, Axis<0> > i;
    ArrayAccessor<Double, Axis<1> > j(value);
    for (j.reset(),l=0; j!=j.end(); j++,l++) {
       i = j;
       same = True;
       for (i.reset(),k=0; i!=i.end(); i++,k++) {
          col[k] = *i;
          if (l==0 || (l!=0 && !::near(col[k],lastInCol[k]))) same = False;
       }
       lastInCol = col;
//
       if (same) {
          for (i.reset(),k=0; i!=i.end(); ++i,k++) {
             *i = lastOutCol[k];
          }
       } else {
          if (abs) {
             makePixelAbsolute(col);
          } else {
             makePixelRelative(col);
          }
//
          for (i.reset(),k=0; i!=i.end(); ++i,k++) {
             *i = col[k];
          }
          lastOutCol = col;
       }
    }
}




void Coordinate::makeWorldAbsolute (Vector<Double>& world) const
{
   DebugAssert(world.nelements()==nWorldAxes(),AipsError);
   world += referenceValue();
}

 
void Coordinate::makeWorldAbsoluteRef (Vector<Double>& world,
                                    const Vector<Double>& refVal) const
{
   DebugAssert(world.nelements()==nWorldAxes(),AipsError);
   DebugAssert(refVal.nelements()==nWorldAxes(),AipsError);
   world += refVal;
}

void Coordinate::makeWorldRelative (Vector<Double>& world) const
{
   DebugAssert(world.nelements()==nWorldAxes(),AipsError);
   world -= referenceValue();
}  

 
void Coordinate::makePixelAbsolute (Vector<Double>& pixel) const
{
   DebugAssert(pixel.nelements()==nPixelAxes(),AipsError);
   pixel += referencePixel();
}

void Coordinate::makePixelRelative (Vector<Double>& pixel) const
{
   DebugAssert(pixel.nelements()==nPixelAxes(),AipsError);
   pixel -= referencePixel();
}
 


Bool Coordinate::setWorldMixRanges (Vector<Double>& worldMin,
                                    Vector<Double>& worldMax,
                                    const IPosition& shape) const
{
   const uInt n = shape.nelements();
   if (n!=nPixelAxes()) {
      set_error("Shape has must be of length nPixelAxes");
      return False;
   }
   AlwaysAssert(nPixelAxes()==nWorldAxes(), AipsError);

// Return defaults if conversion fails

   setDefaultWorldMixRanges(worldMin, worldMax);

// Do conversions 25% off edge of image

   Vector<Double> pMin(n), pMax(n);
   Vector<Double> wMin, wMax;
   for (uInt i=0; i<n; i++) {
      Double s2 = Double(shape(i)) / 2.0;
//
      if (shape(i)==0) {

// shape not known (probably pixel axis in CS removed)

         pMin(i) = referencePixel()(i) - 10.0;
         pMax(i) = referencePixel()(i) + 10.0;
      } else if (shape(i) == 1) {
         pMin(i) = 0 - 10.0;
         pMax(i) = 0 + 10.0;
      } else if (shape(i) > 0) {
         Double n2 = 1.5 * s2;
         pMin(i) = s2 - n2;
         pMax(i) = s2 + n2;
      }
   }
   Bool ok1 = toWorld(wMin, pMin);
   Bool ok2 = toWorld(wMax, pMax);
   if (ok1 && ok2) {
      for (uInt i=0; i<n; i++) {
         if (shape(i) > 0) {             // If shape not known use default value
            worldMin(i) = wMin(i);
            worldMax(i) = wMax(i);
         }
      }
      return True;
   } else {
      return False;
   }
//
   return True;
}

void Coordinate::setDefaultWorldMixRanges (Vector<Double>& worldMin,
                                           Vector<Double>& worldMax) const
{
   const uInt n = nWorldAxes();
   worldMin.resize(n);
   worldMax.resize(n);
   worldMin = -1.0e99;
   worldMax =  1.0e99;
}

Bool Coordinate::setPreferredWorldAxisUnits (const Vector<String>& prefUnits)
//
// The derived class must set the private data
//
{
    if (prefUnits.nelements() != nWorldAxes()) {
	set_error("Wrong number of elements in preferred units vector");
	return False;
    }

// Check units consistency.  New preferred units must be empty
// or dimensionally consistent with actual units

    Vector<String> currUnits = worldAxisUnits();
    const uInt n = nWorldAxes();
    for (uInt i=0; i<n; i++) {
       if (!prefUnits(i).empty()) {
          Unit u0(prefUnits(i));
          Unit u1(currUnits(i));
          if (u0!=u1) {
             set_error("Preferred units are not dimensionally consistent with actual units");
             return False;
          }
       }
    }
    return True;
}


Bool Coordinate::doNearPixel (const Coordinate& other,
                              const Vector<Bool>& thisAxes,
                              const Vector<Bool>& otherAxes,
                              Double tol) const
{
   if (type() != other.type()) {
      set_error("Coordinate types differ");
      return False;
   }
//
   if (allEQ(thisAxes, False) && allEQ(otherAxes, False)) {
      return True;
   }
//
   if (nPixelAxes() != other.nPixelAxes()) {
      set_error("Number of pixel axes differs");
      return False;
   }
   if (nWorldAxes() != other.nWorldAxes()) {
      set_error("Number of world axes differs");
      return False;
   }
//
   const Vector<Double>&  thisRefVal(referenceValue());
   const Vector<Double>& otherRefVal(other.referenceValue());
   const Vector<Double>&  thisInc(increment());
   const Vector<Double>& otherInc(other.increment());
   const Vector<Double>&  thisRefPix(referencePixel());
   const Vector<Double>& otherRefPix(other.referencePixel());
/*
   const Vector<String>&  thisNames(worldAxisNames());
   const Vector<String>& otherNames(other.worldAxisNames());
*/
   const Vector<String>&  thisUnits(worldAxisUnits());
   const Vector<String>& otherUnits(other.worldAxisUnits());
//
   const Matrix<Double>&  thisPC(linearTransform());
   const Matrix<Double>& otherPC(other.linearTransform());
   if (thisPC.nrow() != otherPC.nrow()) {
      set_error ("PC matrices have different numbers of rows");
      return False;
   }
   if (thisPC.ncolumn() != otherPC.ncolumn()) {
      set_error ("PC matrices have different numbers of columns");
      return False;
   }
//
   for (uInt i=0; i<nPixelAxes(); i++) {
      if (thisAxes(i) && otherAxes(i)) {

// Units

         String x1 = thisUnits(i);
         x1.upcase();
         String x2 = otherUnits(i);
         x2.upcase();
//
         Int i1 = x1.index(RXwhite,0);
         if (i1==-1) i1 = x1.length();
         Int i2 = x2.index(RXwhite,0);
         if (i2==-1) i2 = x2.length();
//
         String y1 = String(x1.before(i1));
         String y2 = String(x2.before(i2));
         ostringstream oss;
         if (y1 != y2) {
           oss << "The Coordinates have differing axis units for axis "
               << i;
           set_error(String(oss));
           return False;
         }

// Ref val

         if (!::near(thisRefVal(i), otherRefVal(i), tol)) {
            oss << "The Coordinates have differing reference values for axis "
                 << i;
            set_error(String(oss));
            return False;
         }

// Increment

         if (!::near(thisInc(i), otherInc(i), tol)) {
            oss << "The Coordinates have differing increments for axis "
                 << i;
            set_error(String(oss));
            return False;
         }

// Ref pix
 
         if (!::near(thisRefPix(i), otherRefPix(i), tol)) {
            oss << "The Coordinates have differing reference pixels for axis "
                 << i;
            set_error(String(oss));
            return False;        
         }

// pc matrix. Compare row by row.  An axis will turn up in the PC
// matrix in any row or column with that number. E.g.,
// values pertaining to axis "i" will be found in all
// entries of row "i" and all entries of column "i"
// So just get the ith row and ith column and compare
// PC is always SQUARE
    
         AlwaysAssert(thisPC.nrow()==thisPC.ncolumn(), AipsError);
//   
         Vector<Double> r1 = thisPC.row(i);
         Vector<Double> r2 = otherPC.row(i);
         for (uInt j=0; j<r1.nelements(); j++) {
            if (!::near(r1(j),r2(j),tol)) return False;
         }
//
         Vector<Double> c1 = thisPC.column(i);
         Vector<Double> c2 = otherPC.column(i);
         for (uInt j=0; j<r1.nelements(); j++) {
            if (!::near(c1(j),c2(j),tol)) return False;
         }
      }
   }

//
   return True;
}


