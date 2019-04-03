#ifndef __TEST_HELPERS_H__
#define __TEST_HELPERS_H__

#include <gtest/gtest.h>
#include <memory>
#include "indistandardproperty.h"
#include "defaultdevice.h"

namespace ITH {

///
/// @brief C String Wrapper.
///
/// A lot of INDI's interfaces want a Raw C String in the form of a pointer
/// to char * (and not const char *).
///
/// CStringWrap provides a wrapper for these strings so we can store them
/// into other standard template library containers. 
///
class CStringWrap
{
  public:

  // Delete unused operators for safety.
  CStringWrap() = delete;
  CStringWrap& operator=( const CStringWrap& ) = delete; 

  /// @brief Make a wrapped C String from an std::string
  CStringWrap( const std::string &name_rhs )
  {
    name = std::unique_ptr<char>( new char[ name_rhs.length() + 1 ] );
    strncpy( name.get(), name_rhs.c_str(), name_rhs.length() + 1 );
  }

  /// @brief Make a wrapped C String from another wrapped C String.
  CStringWrap( const CStringWrap& name_rhs )
  {
    size_t length = strlen( name_rhs.get() );
    name = std::unique_ptr<char>( new char[ length + 1 ] );
    strncpy( name.get(), name_rhs.get(), length + 1 );
  }

  /// @brief Get the raw C string
  char* get()
  {
    return name.get();
  }

  /// @brief Get a constant C string if the class is constant.
  const char* get() const
  {
    return name.get();
  }

  private:

  // unique pointer will take care of deallocation and make sure we don't
  // ever do a double deallocation.
  std::unique_ptr<char> name;
};

///
/// @brief Container for INDI name / data values (where data is templated)
/// 
/// INDI's interfaces often take an array of chars and data.  i.e., if I 
/// want to set a switch value with ISNewSwitch I need an array of
/// char ** for the names of the switches and an array of ISState * for
/// the values.
///
/// This class takes name / value pairs in a C++ 11 convenient and safely
/// maps them into the format that INDI calls want.
///
/// Templated so it can be used with ISState and double.
///
template<class T>
class NamesContainer
{
  public:

  using NameDataPair = std::pair< const std::string, const T >;

  /// Thanos snap unused operators.
  NamesContainer() = delete;
  NamesContainer& operator=(const NamesContainer) = delete;

  ///
  /// @brief Create the container with name/ data pairs.
  ///
  /// Example Input (for ISState ) { { "ABORT", ISS_ON } } 
  ///
  NamesContainer( const std::vector< NameDataPair > &input )
  {
    for ( const auto &name_state_pair: input )
    {
      names_cstr.push_back( name_state_pair.first );
      states.push_back( name_state_pair.second );
    }
    // Aliased pointers == have to be careful. Past this
    // point we should never change names_cstr without 
    // updating names_cstr_alias (and we don't).
    for ( auto &c_str: names_cstr )
    {
      names_cstr_alias.push_back(c_str.get());
    }
  }

  ///
  /// @brief Copy Constructor 
  ///
  NamesContainer( const NamesContainer& rhs ) 
    : names_cstr{ rhs.names_cstr },
      states{ rhs.states } 
  {
    // Re-alias the raw pointers so they point to data we own.
    for ( auto &c_str: names_cstr )
    {
      names_cstr_alias.push_back(c_str.get());
    }
  }
  /// @brief Get the names in INDI Friendly format.
  char **getNames() 
  {
    return names_cstr_alias.data();
  }
  /// @brief Data the data (of Type T) in INDI Friendly format.
  T* getData() 
  {
    return states.data();
  }
  /// @brief Get the size so we can pass it to the INDI Interface.
  size_t getSize() 
  {
    return names_cstr.size();
  }

  private:
  // Owns data
  std::vector< CStringWrap > names_cstr;
  std::vector< T > states;
  // Aliases data in names_cstr.
  std::vector< char* > names_cstr_alias;
};

/// @brief INDI/ C++ 11 friendly container of Names and ISStates
using StateData  = NamesContainer<ISState>;

/// @brief INDI/ C++ 11 friendly container of Names and doubles
using NumberData = NamesContainer<double>;

///
/// @brief Turn on an INDI switch.
///
/// example usage: turn on the Abort button.
///
/// ITH::TurnSwitch( driver, "FOCUS_ABORT_MOTION",
///   ITH::StateData{{{"ABORT", ISS_ON }}} );
///
void TurnSwitch(
  INDI::DefaultDevice& driver,
  const char* switchName,
  ITH::StateData deltas )
{
  ASSERT_EQ( true, driver.ISNewSwitch(
      driver.getDeviceName(),
      switchName,
      deltas.getData(),
      deltas.getNames(),
      deltas.getSize()
  ));
}

///
/// @brief Set a number in the INDI Interface
///
/// Example usage: set the Focuser Position to 10000
///
/// ITH::SetNumber( driver, "ABS_FOCUS_POSITION",
///   ITH::NumberData{{{"FOCUS_ABSOLUTE_POSITION", 10000.0 }}} );
///
void SetNumber(
  INDI::DefaultDevice& driver,
  const char* switchName,
  ITH::NumberData deltas )
{
  ASSERT_EQ( true, driver.ISNewNumber(
      driver.getDeviceName(),
      switchName,
      deltas.getData(),
      deltas.getNames(),
      deltas.getSize()
  ));
}

///
/// @brief Wrapper class for standard out capture.
///
/// Wraps the gtest framework's stdout capture.  Mostly here to make sure
/// that we reset singleton state on the Google testing framework if an
/// exception occurs.
/// 
class StdoutCapture
{
  public:

  StdoutCapture( const StdoutCapture& ) = delete;
  StdoutCapture& operator=( const StdoutCapture& ) = delete;

  /// @brief Start capturing stdout.
  StdoutCapture() : released{ false }
  {
    testing::internal::CaptureStdout();
  }
  ///
  /// @brief Reset singleton state in the gtest framework, if needed.
  ///
  /// If somebody's already called getOutput then this does nothing.
  /// If they haven't it gets the output and sends it to stdout.
  ///
  ~StdoutCapture()
  {
    if ( !released )
    {
      // If nobody fetched the output then it goes back to stdout.
      std::cout << getOutput();
    }
  }
  ///
  /// @brief Gets captured output from the google test framework 
  /// 
  std::string getOutput()
  {
    released = true;
    return testing::internal::GetCapturedStdout();
  }
  
  private:
  bool released;
};

///
/// @brief Smartish Pointer wrapper for XMLEle
///
/// The unit test framework can use INDI's lilXML library to parse the output
/// of stdout.  This is a smart wrapper for some of the classes that
/// lilXML returns.  It's responsible for releasing the resources that we
/// get from lilXML (i.e., it owns the data).
///  
class XMLEleWrapper
{
  public:

  XMLEleWrapper() = delete;
  XMLEleWrapper( LilXML *lpArg, XMLEle* rootArg ) : 
      lp{ lpArg }, root{ rootArg}
  {
  }
  /// Do not allow copy operations that would confuse ownership
  XMLEleWrapper( const XMLEleWrapper& ) = delete;
  XMLEleWrapper& operator=( const XMLEleWrapper& ) = delete;
  ~XMLEleWrapper()
  {
    delXMLEle( root );
    delLilXML( lp );
  }
  // non-ownership transferring get function.
  XMLEle* get() { return root ; }

  private:
    LilXML* lp;
    XMLEle* root;
};

// Allow transfer of XMLEleWrappers, but guarantee uniqueness.
using XMLElePtr = std::unique_ptr< XMLEleWrapper >;
using XMLKeyValue = std::pair< std::string, std::string >;

/// @brief Wrap lilxml for exception safety and general niceness.
///
/// Example Usage:
///
/// {
///    ITH::StdoutCapture outCap;   // Start stdout capture
///    ...
///    Driver testing code goes here
///    ...
///    // Create xml capture from stdout
///    ITH::XMLCapture xml( outCap.getOutput() ); 
///    // Make sure that ABS_FOCUS_POSITION was set to "Busy"
///    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Busy" );
/// }
///
class XMLCapture
{
  public:

    XMLCapture( void ) = delete;

    XMLCapture( const std::string& text )
    {
      //
      // Create a vector of XMLElePtr Elements by parsing text
      //
      std::vector<XMLElePtr> rootElements;
      LilXML *lp = nullptr;
      char errMsg[ 1024 ];

      for ( char c : text ) 
      {
        if ( lp == nullptr )
        {
          lp = newLilXML();
        }
        XMLEle *root = readXMLEle( lp, c, errMsg );
        if ( root ) 
        {
          rootElements.push_back( XMLElePtr(new XMLEleWrapper( lp, root )) );
          lp = nullptr;
        }
      }
      if ( lp ) delLilXML( lp );

      // 
      // Convert the root XMLEle Elements to data my test is interested in.
      //
      for( auto& ele: rootElements )
      {
        XMLEle* root = ele.get()->get();
        XMLEle* ep;
        {
          std::string name;
          std::string status;
          for ( XMLAtt* at = nextXMLAtt( root, 1 ); at != nullptr; at = nextXMLAtt( root, 0 ))
          {
            if ( std::string( nameXMLAtt(at)) == "name" )
            {
              name = std::string( valuXMLAtt( at ));
            }
            if ( std::string( nameXMLAtt(at)) == "state" )
            {
              status= std::string( valuXMLAtt( at ));
            }
          }
          if ( name != "" and status != "" )
          {
            xmlData.push_back( XMLKeyValue( name, status ));
          }
        }
        for ( ep = nextXMLEle( root,1 ); ep != nullptr; ep = nextXMLEle( root, 0 )) {
          std::string value( pcdataXMLEle( ep ));
          for ( XMLAtt* at = nextXMLAtt( ep, 1 ); at != nullptr; at = nextXMLAtt( ep, 0 ))
          {
            if ( std::string( nameXMLAtt(at)) == "name" )
            {
              std::string key( valuXMLAtt( at ));
              xmlData.push_back( XMLKeyValue( key, value ));
            }
          }
        }
      }
    }

    std::string lastState( const std::string& key )
    {
      std::string rval;
      for ( const auto& entry : xmlData )
      {
        if ( entry.first == key )
        {
          rval = entry.second;
        }
      }
      return rval;
    }

  private:
    std::vector<XMLKeyValue> xmlData;
};
}

#endif

