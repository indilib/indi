#ifndef __SIMPLE_OSTREAM__
#define __SIMPLE_OSTREAM__

#include <cstddef>  // for std::size_t
#include <string.h>
#include <string>
#include <type_traits>
#include "basic_types.h"  // for BeeFocus::IpAddress.

// Like std::enable_if_t, but works in C++ 11.
template< bool B, class T = void >
using my_enable_if_t = typename std::enable_if<B,T>::type;

// Create a trait for the focuser ostreams, so we don't accidentally
// try to use these templates on other ostreams (like std::ostream).
template< class T>
struct is_beefocus_ostream : std::integral_constant< bool, false > {};


// Specialize the classes that are allowed to use this interface.
// TODO - I should move this to the class definition
// TODO - Learn more template code.
class WifiDebugOstream;
class DebugInterface;
class NetInterface;
class WifiConnectionEthernet;

template<> 
struct is_beefocus_ostream<WifiDebugOstream> : std::integral_constant< bool, true > {};

template<> 
struct is_beefocus_ostream<DebugInterface> : std::integral_constant< bool, true > {};

template<> 
struct is_beefocus_ostream<NetInterface> : std::integral_constant< bool, true > {};

template<> 
struct is_beefocus_ostream<WifiConnectionEthernet> : std::integral_constant< bool, true > {};


/// @brief Output a buffer
template <class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
void rawWrite( T& ostream,  const char *bytes, std::size_t numBytes )
{
   for ( size_t i = 0; i < numBytes; ++i )
   {
      ostream << bytes[i];
   }
}

/// @brief Output a WIFI IP address
template <class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, const BeeFocus::IpAddress& address )
{
  ostream << address[0] << "." << address[1] << "." << address[2] << "." << address[3];
  return ostream;
}

/// @brief Output a C style string.
template <class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, const char* string )
{
  std::size_t length = strlen( string );
  rawWrite( ostream, string, length );
  return ostream;
} 

/// @brief Output an unsigned number of a SIMPLE_ISTREAM.
template <class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, unsigned int i )
{
  // Handle digits that aren't the lowest digit (if any)
  if ( i >= 10 )
  {
    ostream << i/10;
  }

  // Handle the lowest digit
  ostream << (char) ('0' + (i % 10));
  return ostream;
}


/// @brief Output an signed number of a SIMPLE_ISTREAM.
template<class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, int i )
{
  // Handle -
  if ( i < 0 )
  {
    ostream << "-";   
    i = -i;
  }

  ostream << (unsigned int) i;
  return ostream;
}  

/// @brief Output an std::string
template<class T, 
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, const std::string& string )
{
  ostream << string.c_str();
  return ostream;
}

#endif

