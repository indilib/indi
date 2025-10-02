/*******************************************************************************
 Copyright(c) 2020 Eric Dejouhanet. All rights reserved.
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <gtest/gtest.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>

#include "basedevice.h"

#include "indiproperty.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"
#include "indipropertyblob.h"

#include <indipropertyview.h>

TEST(CORE_PROPERTY_CLASS, Test_EmptyProperty)
{
    INDI::Property p;

    ASSERT_EQ(p.getProperty(), nullptr);
    ASSERT_EQ(p.getBaseDevice(), nullptr);
    ASSERT_EQ(p.getType(), INDI_UNKNOWN);
    ASSERT_EQ(p.getRegistered(), false);
    ASSERT_EQ(p.isDynamic(), false);

    ASSERT_EQ(p.getName(), nullptr);
    ASSERT_EQ(p.getLabel(), nullptr);
    ASSERT_EQ(p.getGroupName(), nullptr);
    ASSERT_EQ(p.getDeviceName(), nullptr);
    ASSERT_EQ(p.getTimestamp(), nullptr);

    ASSERT_EQ(p.getState(), IPS_ALERT);
    ASSERT_EQ(p.getPermission(), IP_RO);

    ASSERT_EQ(p.getNumber(), nullptr);
    ASSERT_EQ(p.getText(), nullptr);
    ASSERT_EQ(p.getSwitch(), nullptr);
    ASSERT_EQ(p.getLight(), nullptr);
    ASSERT_EQ(p.getBLOB(), nullptr);
}

TEST(CORE_PROPERTY_CLASS, Test_PropertySetters)
{
    INDI::Property p;

    INDI::PropertyViewNumber nvp;
    nvp.setDeviceName("device field");
    nvp.setName("name field");
    nvp.setLabel("label field");
    nvp.setGroupName("group field");
    nvp.setPermission(IP_RW);
    nvp.setState(IPS_BUSY);
    nvp.setTimestamp("timestamp field");
    nvp.setAux(nullptr);
    nvp.setWidgets(nullptr, 0);
    nvp.setTimeout(42);


    // Setting a property makes it registered but NOT meaningful
    p.setProperty(&nvp);
    ASSERT_EQ(p.getProperty(), &nvp);
    ASSERT_EQ(p.getType(), INDI_UNKNOWN);
    ASSERT_EQ(p.getNumber(), nullptr);

    // Property fields remain unpropagated
    ASSERT_EQ(p.getName(), nullptr);
    ASSERT_EQ(p.getLabel(), nullptr);
    ASSERT_EQ(p.getGroupName(), nullptr);
    ASSERT_EQ(p.getDeviceName(), nullptr);
    ASSERT_EQ(p.getTimestamp(), nullptr);

    // Other fields remain unchanged
    ASSERT_EQ(p.getRegistered(), true);
    ASSERT_EQ(p.isDynamic(), false);
    ASSERT_EQ(p.getBaseDevice(), nullptr);
    ASSERT_EQ(p.getState(), IPS_ALERT);
    ASSERT_EQ(p.getPermission(), IP_RO);

    // Other specific conversions return nothing
    ASSERT_EQ(p.getText(), nullptr);
    ASSERT_EQ(p.getSwitch(), nullptr);
    ASSERT_EQ(p.getLight(), nullptr);
    ASSERT_EQ(p.getBLOB(), nullptr);

    // Setting a property type gives a meaning to the property
    // Note the possible desync between property value and type
    p.setType(INDI_NUMBER);
    ASSERT_EQ(p.getProperty(), &nvp);
    ASSERT_EQ(p.getNumber(), &nvp);
    ASSERT_EQ(p.getType(), INDI_NUMBER);

    // Property fields are propagated
    ASSERT_STREQ(p.getName(), "name field");
    ASSERT_STREQ(p.getLabel(), "label field");
    ASSERT_STREQ(p.getGroupName(), "group field");
    ASSERT_STREQ(p.getDeviceName(), "device field");
    ASSERT_STREQ(p.getTimestamp(), "timestamp field");

    // And previously set fields remain unchanged
    ASSERT_EQ(p.getRegistered(), true);
    ASSERT_EQ(p.isDynamic(), false);
    ASSERT_EQ(p.getBaseDevice(), nullptr);

    // Other specific conversions still return nothing
    ASSERT_EQ(p.getText(), nullptr);
    ASSERT_EQ(p.getSwitch(), nullptr);
    ASSERT_EQ(p.getLight(), nullptr);
    ASSERT_EQ(p.getBLOB(), nullptr);

    // Clearing a property brings it back to the unregistered state
    p.setProperty(nullptr);
    ASSERT_EQ(p.getProperty(), nullptr);
    ASSERT_EQ(p.getType(), INDI_UNKNOWN);
    ASSERT_EQ(p.getRegistered(), false);

    // Property fields are not propagated anymore
    ASSERT_EQ(p.getName(), nullptr);
    ASSERT_EQ(p.getLabel(), nullptr);
    ASSERT_EQ(p.getGroupName(), nullptr);
    ASSERT_EQ(p.getDeviceName(), nullptr);
    ASSERT_EQ(p.getTimestamp(), nullptr);

    // And other fields are reset
    ASSERT_EQ(p.isDynamic(), false);
    ASSERT_EQ(p.getBaseDevice(), nullptr);
    ASSERT_EQ(p.getState(), IPS_ALERT);
    ASSERT_EQ(p.getPermission(), IP_RO);

    // Again, conversions return nothing
    ASSERT_EQ(p.getNumber(), nullptr);
    ASSERT_EQ(p.getText(), nullptr);
    ASSERT_EQ(p.getSwitch(), nullptr);
    ASSERT_EQ(p.getLight(), nullptr);
    ASSERT_EQ(p.getBLOB(), nullptr);
}

TEST(CORE_PROPERTY_CLASS, DISABLED_Test_Integrity)
{
    INDI::Property p;

    INumberVectorProperty * corrupted_property = (INumberVectorProperty*) (void*) 0x12345678;
    //INDI::BaseDevice * corrupted_device = (INDI::BaseDevice*) (void*) 0x87654321;
    INDI::BaseDevice corrupted_device;

    p.setProperty(corrupted_property);

    // A magic header should protect the property from returning garbage
    EXPECT_EQ(p.getProperty(), nullptr);
    EXPECT_EQ(p.getRegistered(), false);

    // A verification mechanism should protect the property from getting an incorrect type
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);
    p.setType(INDI_NUMBER);
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);
    p.setType(INDI_TEXT);
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);
    p.setType(INDI_SWITCH);
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);
    p.setType(INDI_LIGHT);
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);
    p.setType(INDI_BLOB);
    EXPECT_EQ(p.getType(), INDI_UNKNOWN);

    // A verification mechanism should protect the property from being converted to an incorrect type
    EXPECT_EQ(p.getNumber(), nullptr);
    EXPECT_EQ(p.getText(), nullptr);
    EXPECT_EQ(p.getSwitch(), nullptr);
    EXPECT_EQ(p.getLight(), nullptr);
    EXPECT_EQ(p.getBLOB(), nullptr);

    // A verification mechanism should protect the property from being associated to an invalid device
    p.setBaseDevice(corrupted_device);
    EXPECT_EQ(p.getBaseDevice(), nullptr);
}


TEST(CORE_PROPERTY_CLASS, Test_PropertyNumber)
{
    INDI::PropertyNumber p{1};

    p[0].setName("widget name");
    p[0].setLabel("widget label");
    p[0].setValue(4);
    p[0].setMinMax(-10, 10);

    p.setDeviceName("property device");
    p.setName("property name");
    p.setLabel("property label");
    p.setGroupName("property group");
    p.setPermission(IP_RW);
    p.setTimeout(1000);
    p.setState(IPS_OK);

    ASSERT_STREQ(p[0].getName(),  "widget name");
    ASSERT_STREQ(p[0].getLabel(), "widget label");
    ASSERT_EQ(p[0].getValue(),  4);
    ASSERT_EQ(p[0].getMin(), -10);
    ASSERT_EQ(p[0].getMax(),  10);

    ASSERT_STREQ(p.getDeviceName(), "property device");
    ASSERT_STREQ(p.getName(),       "property name");
    ASSERT_STREQ(p.getLabel(),      "property label");
    ASSERT_STREQ(p.getGroupName(),  "property group");
    ASSERT_EQ(p.getPermission(), IP_RW);
    ASSERT_EQ(p.getTimeout(), 1000);
    ASSERT_EQ(p.getState(), IPS_OK);

    // change values and test
    p[0].setName("widget other name");
    p[0].setLabel("widget other label");
    p[0].setValue(40);
    p[0].setMinMax(-100, 100);

    p.setDeviceName("property other device");
    p.setName("property other name");
    p.setLabel("property other label");
    p.setGroupName("property other group");
    p.setPermission(IP_RO);
    p.setTimeout(500);
    p.setState(IPS_ALERT);

    ASSERT_STREQ(p[0].getName(),  "widget other name");
    ASSERT_STREQ(p[0].getLabel(), "widget other label");
    ASSERT_EQ(p[0].getValue(),  40);
    ASSERT_EQ(p[0].getMin(), -100);
    ASSERT_EQ(p[0].getMax(),  100);

    ASSERT_STREQ(p.getDeviceName(), "property other device");
    ASSERT_STREQ(p.getName(),       "property other name");
    ASSERT_STREQ(p.getLabel(),      "property other label");
    ASSERT_STREQ(p.getGroupName(),  "property other group");
    ASSERT_EQ(p.getPermission(), IP_RO);
    ASSERT_EQ(p.getTimeout(), 500);
    ASSERT_EQ(p.getState(), IPS_ALERT);

    ASSERT_EQ(INDI::PropertyNumber(INDI::Property(p)).isValid(), true);
    ASSERT_EQ(INDI::PropertySwitch(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyText(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyLight(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyBlob(INDI::Property(p)).isValid(), false);
}

TEST(CORE_PROPERTY_CLASS, Test_PropertySwitch)
{
    INDI::PropertySwitch p{1};

    p[0].setName("widget name");
    p[0].setLabel("widget label");
    p[0].setState(ISS_ON);

    p.setDeviceName("property device");
    p.setName("property name");
    p.setLabel("property label");
    p.setGroupName("property group");
    p.setPermission(IP_RW);
    p.setTimeout(1000);
    p.setState(IPS_OK);

    ASSERT_STREQ(p[0].getName(),  "widget name");
    ASSERT_STREQ(p[0].getLabel(), "widget label");
    ASSERT_EQ(p[0].getState(),  ISS_ON);

    ASSERT_STREQ(p.getDeviceName(), "property device");
    ASSERT_STREQ(p.getName(),       "property name");
    ASSERT_STREQ(p.getLabel(),      "property label");
    ASSERT_STREQ(p.getGroupName(),  "property group");
    ASSERT_EQ(p.getPermission(), IP_RW);
    ASSERT_EQ(p.getTimeout(), 1000);
    ASSERT_EQ(p.getState(), IPS_OK);

    // change values and test
    p[0].setName("widget other name");
    p[0].setLabel("widget other label");
    p[0].setState(ISS_OFF);

    p.setDeviceName("property other device");
    p.setName("property other name");
    p.setLabel("property other label");
    p.setGroupName("property other group");
    p.setPermission(IP_RO);
    p.setTimeout(500);
    p.setState(IPS_ALERT);

    ASSERT_STREQ(p[0].getName(),  "widget other name");
    ASSERT_STREQ(p[0].getLabel(), "widget other label");
    ASSERT_EQ(p[0].getState(),  ISS_OFF);

    ASSERT_STREQ(p.getDeviceName(), "property other device");
    ASSERT_STREQ(p.getName(),       "property other name");
    ASSERT_STREQ(p.getLabel(),      "property other label");
    ASSERT_STREQ(p.getGroupName(),  "property other group");
    ASSERT_EQ(p.getPermission(), IP_RO);
    ASSERT_EQ(p.getTimeout(), 500);
    ASSERT_EQ(p.getState(), IPS_ALERT);

    ASSERT_EQ(INDI::PropertyNumber(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertySwitch(INDI::Property(p)).isValid(), true);
    ASSERT_EQ(INDI::PropertyText(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyLight(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyBlob(INDI::Property(p)).isValid(), false);
}

TEST(CORE_PROPERTY_CLASS, Test_PropertyText)
{
    INDI::PropertyText p{1};

    p[0].setName("widget name");
    p[0].setLabel("widget label");
    p[0].setText("widget text");

    p.setDeviceName("property device");
    p.setName("property name");
    p.setLabel("property label");
    p.setGroupName("property group");
    p.setPermission(IP_RW);
    p.setTimeout(1000);
    p.setState(IPS_OK);

    ASSERT_STREQ(p[0].getName(),  "widget name");
    ASSERT_STREQ(p[0].getLabel(), "widget label");
    ASSERT_STREQ(p[0].getText(),  "widget text");

    ASSERT_STREQ(p.getDeviceName(), "property device");
    ASSERT_STREQ(p.getName(),       "property name");
    ASSERT_STREQ(p.getLabel(),      "property label");
    ASSERT_STREQ(p.getGroupName(),  "property group");
    ASSERT_EQ(p.getPermission(), IP_RW);
    ASSERT_EQ(p.getTimeout(), 1000);
    ASSERT_EQ(p.getState(), IPS_OK);

    // change values and test
    p[0].setName("widget other name");
    p[0].setLabel("widget other label");
    p[0].setText("widget other text");

    p.setDeviceName("property other device");
    p.setName("property other name");
    p.setLabel("property other label");
    p.setGroupName("property other group");
    p.setPermission(IP_RO);
    p.setTimeout(500);
    p.setState(IPS_ALERT);

    ASSERT_STREQ(p[0].getName(),  "widget other name");
    ASSERT_STREQ(p[0].getLabel(), "widget other label");
    ASSERT_STREQ(p[0].getText(),  "widget other text");

    ASSERT_STREQ(p.getDeviceName(), "property other device");
    ASSERT_STREQ(p.getName(),       "property other name");
    ASSERT_STREQ(p.getLabel(),      "property other label");
    ASSERT_STREQ(p.getGroupName(),  "property other group");
    ASSERT_EQ(p.getPermission(), IP_RO);
    ASSERT_EQ(p.getTimeout(), 500);
    ASSERT_EQ(p.getState(), IPS_ALERT);

    ASSERT_EQ(INDI::PropertyNumber(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertySwitch(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyText(INDI::Property(p)).isValid(), true);
    ASSERT_EQ(INDI::PropertyLight(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyBlob(INDI::Property(p)).isValid(), false);
}

TEST(CORE_PROPERTY_CLASS, Test_PropertyLight)
{
    INDI::PropertyLight p{1};

    p[0].setName("widget name");
    p[0].setLabel("widget label");
    p[0].setState(IPS_OK);

    p.setDeviceName("property device");
    p.setName("property name");
    p.setLabel("property label");
    p.setGroupName("property group");
    p.setPermission(IP_RW);
    p.setTimeout(1000);
    p.setState(IPS_OK);

    ASSERT_STREQ(p[0].getName(),  "widget name");
    ASSERT_STREQ(p[0].getLabel(), "widget label");
    ASSERT_EQ(p[0].getState(),  IPS_OK);

    ASSERT_STREQ(p.getDeviceName(), "property device");
    ASSERT_STREQ(p.getName(),       "property name");
    ASSERT_STREQ(p.getLabel(),      "property label");
    ASSERT_STREQ(p.getGroupName(),  "property group");
    ASSERT_EQ(p.getPermission(), IP_RO); // cannot change
    ASSERT_EQ(p.getTimeout(), 0); // cannot change
    ASSERT_EQ(p.getState(), IPS_OK);

    // change values and test
    p[0].setName("widget other name");
    p[0].setLabel("widget other label");
    p[0].setState(IPS_OK);

    p.setDeviceName("property other device");
    p.setName("property other name");
    p.setLabel("property other label");
    p.setGroupName("property other group");
    p.setPermission(IP_RO);
    p.setTimeout(500);
    p.setState(IPS_ALERT);

    ASSERT_STREQ(p[0].getName(),  "widget other name");
    ASSERT_STREQ(p[0].getLabel(), "widget other label");
    ASSERT_EQ(p[0].getState(),  IPS_OK);

    ASSERT_STREQ(p.getDeviceName(), "property other device");
    ASSERT_STREQ(p.getName(),       "property other name");
    ASSERT_STREQ(p.getLabel(),      "property other label");
    ASSERT_STREQ(p.getGroupName(),  "property other group");
    ASSERT_EQ(p.getPermission(), IP_RO); // cannot change
    ASSERT_EQ(p.getTimeout(), 0);
    ASSERT_EQ(p.getState(), IPS_ALERT);

    ASSERT_EQ(INDI::PropertyNumber(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertySwitch(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyText(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyLight(INDI::Property(p)).isValid(), true);
    ASSERT_EQ(INDI::PropertyBlob(INDI::Property(p)).isValid(), false);
}

TEST(CORE_PROPERTY_CLASS, Test_PropertyBlob)
{
    INDI::PropertyBlob p{1};

    p[0].setName("widget name");
    p[0].setLabel("widget label");
    p[0].setBlob(nullptr);
    p[0].setBlobLen(8);
    p[0].setSize(16);
    p[0].setFormat("format");

    p.setDeviceName("property device");
    p.setName("property name");
    p.setLabel("property label");
    p.setGroupName("property group");
    p.setPermission(IP_RW);
    p.setTimeout(1000);
    p.setState(IPS_OK);

    ASSERT_STREQ(p[0].getName(),  "widget name");
    ASSERT_STREQ(p[0].getLabel(), "widget label");
    ASSERT_EQ(p[0].getBlob(), nullptr);
    ASSERT_EQ(p[0].getBlobLen(), 8);
    ASSERT_EQ(p[0].getSize(), 16);
    ASSERT_STREQ(p[0].getFormat(), "format");

    ASSERT_STREQ(p.getDeviceName(), "property device");
    ASSERT_STREQ(p.getName(),       "property name");
    ASSERT_STREQ(p.getLabel(),      "property label");
    ASSERT_STREQ(p.getGroupName(),  "property group");
    ASSERT_EQ(p.getPermission(), IP_RW);
    ASSERT_EQ(p.getTimeout(), 1000);
    ASSERT_EQ(p.getState(), IPS_OK);

    // change values and test
    p[0].setName("widget other name");
    p[0].setLabel("widget other label");
    p[0].setBlob(reinterpret_cast<void*>(0x10));
    p[0].setBlobLen(16);
    p[0].setSize(32);
    p[0].setFormat("format 2");

    p.setDeviceName("property other device");
    p.setName("property other name");
    p.setLabel("property other label");
    p.setGroupName("property other group");
    p.setPermission(IP_RO);
    p.setTimeout(500);
    p.setState(IPS_ALERT);

    ASSERT_STREQ(p[0].getName(),  "widget other name");
    ASSERT_STREQ(p[0].getLabel(), "widget other label");
    ASSERT_EQ(p[0].getBlob(), reinterpret_cast<void*>(0x10));
    ASSERT_EQ(p[0].getBlobLen(), 16);
    ASSERT_EQ(p[0].getSize(), 32);
    ASSERT_STREQ(p[0].getFormat(), "format 2");

    ASSERT_STREQ(p.getDeviceName(), "property other device");
    ASSERT_STREQ(p.getName(),       "property other name");
    ASSERT_STREQ(p.getLabel(),      "property other label");
    ASSERT_STREQ(p.getGroupName(),  "property other group");
    ASSERT_EQ(p.getPermission(), IP_RO);
    ASSERT_EQ(p.getTimeout(), 500);
    ASSERT_EQ(p.getState(), IPS_ALERT);

    ASSERT_EQ(INDI::PropertyNumber(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertySwitch(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyText(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyLight(INDI::Property(p)).isValid(), false);
    ASSERT_EQ(INDI::PropertyBlob(INDI::Property(p)).isValid(), true);
}
