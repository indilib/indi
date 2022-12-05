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
typedef SSIZE_T ssize_t;
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
