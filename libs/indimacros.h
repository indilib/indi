/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#if defined(_MSC_VER)
# define _USE_MATH_DEFINES
#endif
#include <math.h>

// ============================================================
// Astronomical / physical constants
// ============================================================
#ifndef J2000
# define J2000                2451545.0
#endif
#ifndef ERRMSG_SIZE
# define ERRMSG_SIZE          1024
#endif
#ifndef STELLAR_DAY
# define STELLAR_DAY          86164.098903691
#endif
#ifndef TRACKRATE_SIDEREAL
# define TRACKRATE_SIDEREAL   ((360.0 * 3600.0) / STELLAR_DAY)
#endif
#ifndef SOLAR_DAY
# define SOLAR_DAY            86400
#endif
#ifndef TRACKRATE_SOLAR
# define TRACKRATE_SOLAR      ((360.0 * 3600.0) / SOLAR_DAY)
#endif
#ifndef TRACKRATE_LUNAR
# define TRACKRATE_LUNAR      14.511415
#endif
#ifndef EARTHRADIUSEQUATORIAL
# define EARTHRADIUSEQUATORIAL 6378137.0
#endif
#ifndef EARTHRADIUSPOLAR
# define EARTHRADIUSPOLAR     6356752.0
#endif
#ifndef EARTHRADIUSMEAN
# define EARTHRADIUSMEAN      6372797.0
#endif
#ifndef SUNMASS
# define SUNMASS              1.98847E+30
#endif
#ifndef PLANK_H
# define PLANK_H              6.62607015E-34
#endif
#ifndef DIRAC_H
# define DIRAC_H              (PLANK_H / (2 * M_PI))
#endif
#ifndef EINSTEIN_G
# define EINSTEIN_G           6.67408E-11
#endif
#ifndef EULER
# define EULER                2.71828182845904523536028747135266249775724709369995
#endif
#ifndef ROOT2
# define ROOT2                1.41421356237309504880168872420969807856967187537694
#endif
#ifndef AIRY
# define AIRY                 1.21966
#endif
#ifndef ASTRONOMICALUNIT
# define ASTRONOMICALUNIT     1.495978707E+11
#endif
#ifndef LIGHTSPEED
# define LIGHTSPEED           299792458.0
#endif

// ============================================================
// Angular / arc unit conversion macros
// ============================================================
#ifndef CIRCLE_DEG
# define CIRCLE_DEG           360
#endif
#ifndef CIRCLE_AM
# define CIRCLE_AM            (CIRCLE_DEG * 60)
#endif
#ifndef CIRCLE_AS
# define CIRCLE_AS            (CIRCLE_AM * 60)
#endif
#ifndef RAD_AS
# define RAD_AS               (CIRCLE_AS / (M_PI * 2))
#endif

#ifndef HOURS_TO_DEG
# define HOURS_TO_DEG(h)      ((h) * 15.0)
#endif
#ifndef DEG_TO_HOURS
# define DEG_TO_HOURS(d)      ((d) / 15.0)
#endif
#ifndef ARCMIN_TO_DEG
# define ARCMIN_TO_DEG(am)    ((am) / 60.0)
#endif
#ifndef DEG_TO_ARCMIN
# define DEG_TO_ARCMIN(d)     ((d) * 60.0)
#endif
#ifndef ARCSEC_TO_DEG
# define ARCSEC_TO_DEG(as)    ((as) / 3600.0)
#endif
#ifndef DEG_TO_ARCSEC
# define DEG_TO_ARCSEC(d)     ((d) * 3600.0)
#endif
#ifndef HOURS_TO_RAD
# define HOURS_TO_RAD(h)      ((h) * M_PI / 12.0)
#endif
#ifndef RAD_TO_HOURS
# define RAD_TO_HOURS(r)      ((r) * 12.0 / M_PI)
#endif
#ifndef DEG_TO_RAD
# define DEG_TO_RAD(d)        ((d) * M_PI / 180.0)
#endif
#ifndef RAD_TO_DEG
# define RAD_TO_DEG(r)        ((r) * 180.0 / M_PI)
#endif

// ============================================================
// Derived astronomical constants (depend on the above)
// ============================================================
#ifndef PARSEC
# define PARSEC               (ASTRONOMICALUNIT * RAD_AS)
#endif
#ifndef JULIAN_LY
# define JULIAN_LY            (LIGHTSPEED * SOLAR_DAY * 365)
#endif
#ifndef STELLAR_LY
# define STELLAR_LY           (LIGHTSPEED * STELLAR_DAY * 365)
#endif

// ============================================================
// Photometry macros
// ============================================================
#ifndef FLUX
# define FLUX(wavelength)     ((wavelength) / (PLANK_H * LIGHTSPEED))
#endif
#ifndef CANDLE
# define CANDLE               ((1.0 / 683.0) * FLUX(555))
#endif
#ifndef LUMEN
# define LUMEN(wavelength)    (CANDLE / (4 * M_PI) * pow((FLUX(wavelength) / FLUX(555)), 0.25))
#endif
#ifndef REDSHIFT
# define REDSHIFT(wavelength, reference) (1.0 - ((reference) / (wavelength)))
#endif
#ifndef DOPPLER
# define DOPPLER(shift, speed) ((speed) * (shift))
#endif

/**
 * @brief Unused variable
 *
 * Indicates to the compiler that the parameter with the specified name is not used in the body of a function.
 * This can be used to suppress compiler warnings while allowing functions to be defined with meaningful
 * parameter names in their signatures.
 *
 */
#ifndef INDI_UNUSED
# define INDI_UNUSED(x) (void)x
#endif

/**
 * @brief The POSIX C type "ssize_t" is available on Unix-like systems in unistd.h.
 * For Windows Visual Studio based compilers without unistd.h, the following stanza allows use of ssize_t.
 */
#ifdef _MSC_VER
# include <BaseTsd.h>
#ifndef HAVE_SSIZE_T
typedef SSIZE_T ssize_t;
#endif
#else
# include <unistd.h>
#endif

/**
 * @brief Expressions to test whether the attribute is recognized by the C++ compiler
 */
#ifndef INDI_HAS_CPP_ATTRIBUTE
# ifdef __has_cpp_attribute
#   define INDI_HAS_CPP_ATTRIBUTE(x)  __has_cpp_attribute(x)
# else
#   define INDI_HAS_CPP_ATTRIBUTE(x)  0
# endif
#endif

/**
 * @brief Expressions to test whether the attribute is recognized by the C compiler
 */
#ifndef INDI_HAS_ATTRIBUTE
# ifdef __has_attribute
#   define INDI_HAS_ATTRIBUTE(x)      __has_attribute(x)
# else
#   define INDI_HAS_ATTRIBUTE(x)   0
# endif
#endif

/**
 * @brief Allows attributes to be set on null statements
 *
 * The fallthrough attribute with a null statement serves as a fallthrough statement.
 * It hints to the compiler that a statement that falls through to another case label,
 * or user-defined label in a switch statement is intentional and thus the
 * -Wimplicit-fallthrough warning must not trigger.
 * The fallthrough attribute may appear at most once in each attribute list,
 * and may not be mixed with other attributes.
 * It can only be used in a switch statement (the compiler will issue an error otherwise),
 * after a preceding statement and before a logically succeeding case label, or user-defined label.
 *
 * switch (cond)
 * {
 * case 1:
 *   bar (1);
 *   INDI_FALLTHROUGH;
 * case 2:
 *   ...
 * }
 */
#ifndef INDI_FALLTHROUGH
# if defined(__cplusplus)
#  if INDI_HAS_CPP_ATTRIBUTE(clang::fallthrough)
#   define INDI_FALLTHROUGH [[clang::fallthrough]]
#  elif INDI_HAS_CPP_ATTRIBUTE(gnu::fallthrough)
#   define INDI_FALLTHROUGH [[gnu::fallthrough]]
#  elif INDI_HAS_CPP_ATTRIBUTE(fallthrough)
#   define INDI_FALLTHROUGH [[fallthrough]]
#  endif
# else
#  if INDI_HAS_ATTRIBUTE(fallthrough)
#   define INDI_FALLTHROUGH __attribute__((fallthrough))
#  else
#   define INDI_FALLTHROUGH do {} while (0)
#  endif
# endif
#endif

/**
 * @brief Opaque pointer
 *
 * The D_PTR macro is part of a design pattern called the d-pointer (also called the opaque pointer)
 * where the implementation details of a library may be hidden from its users and changes to the implementation
 * can be made to a library without breaking binary compatibility.
 */
#if defined(__cplusplus)

#include <memory>

template <typename T>
static inline std::shared_ptr<T> make_shared_weak(T *object)
{
    return std::shared_ptr<T>(object, [](T*) {});
}

template <typename T>
static inline T *getPtrHelper(T *ptr)
{
    return ptr;
}

template <typename Wrapper>
static inline typename Wrapper::element_type *getPtrHelper(const Wrapper &p)
{
    return p.get();
}

#define DECLARE_PRIVATE(Class) \
    inline Class##Private* d_func() { return reinterpret_cast<Class##Private *>(getPtrHelper(this->d_ptr)); } \
    inline const Class##Private* d_func() const { return reinterpret_cast<const Class##Private *>(getPtrHelper(this->d_ptr)); } \
    friend Class##Private;

#define DECLARE_PRIVATE_D(Dptr, Class) \
    inline Class##Private* d_func() { return reinterpret_cast<Class##Private *>(getPtrHelper(Dptr)); } \
    inline const Class##Private* d_func() const { return reinterpret_cast<const Class##Private *>(getPtrHelper(Dptr)); } \
    friend Class##Private;

#define D_PTR(Class) Class##Private * const d = d_func()

#endif

// enable warnings for printf-style functions
#ifndef ATTRIBUTE_FORMAT_PRINTF
# if INDI_HAS_ATTRIBUTE(format)
#  define ATTRIBUTE_FORMAT_PRINTF(A, B) __attribute__((format(printf, (A), (B))))
# else
#  define ATTRIBUTE_FORMAT_PRINTF(A, B)
# endif
#endif

#ifdef SWIG
# define INDI_DEPRECATED(message)
#elif __cplusplus
# define INDI_DEPRECATED(message) [[deprecated(message)]]
#else
# define INDI_DEPRECATED(message) __attribute__ ((deprecated))
#endif
