#ifndef __BASIC_TYPES_H__
#define __BASIC_TYPES_H__

#include <array>

namespace BeeFocus
{
	const int noValue= -1;
	using IpAddress = std::array<int,4>;

  ///
  /// @brief Add 1 to an enum from an enum class (wee hacky)
  ///
  /// @param[in] ENUM     - the Enum Type
  /// @param[in] MAX_VAL  - the maximum value in the enum class
  /// @param[in] e        - the enum to increment.
  ///
  template< typename ENUM, ENUM MAX_VAL >
  ENUM& advance(ENUM& e )
  {
    if ( e != MAX_VAL )
    {
      // wee hacky
      e = static_cast<ENUM>( static_cast<int>(e) + 1 );
    }
    return e;
  }
};



#endif

