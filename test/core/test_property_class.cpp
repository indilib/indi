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

TEST(CORE_PROPERTY_CLASS, Test_EmptyProperty)
{
    INDI::Property p;

    ASSERT_FALSE(p.getBaseDevice());
    ASSERT_FALSE(p.getBaseDevice().isValid());
    ASSERT_EQ(p.getBaseDevice(), nullptr); // deprecated

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

    ASSERT_FALSE(p.getNumber());
    ASSERT_FALSE(p.getNumber().isValid());
    ASSERT_EQ(p.getNumber(), nullptr); // deprecated

    ASSERT_FALSE(p.getText());
    ASSERT_FALSE(p.getText().isValid());
    ASSERT_EQ(p.getText(), nullptr); // deprecated

    ASSERT_FALSE(p.getSwitch());
    ASSERT_FALSE(p.getSwitch().isValid());
    ASSERT_EQ(p.getSwitch(), nullptr); // deprecated

    ASSERT_FALSE(p.getLight());
    ASSERT_FALSE(p.getLight().isValid());
    ASSERT_EQ(p.getLight(), nullptr); // deprecated

    ASSERT_FALSE(p.getBLOB());
    ASSERT_FALSE(p.getBLOB().isValid());
    ASSERT_EQ(p.getBLOB(), nullptr); // deprecated
}

TEST(CORE_PROPERTY_CLASS, Test_PropertySetters)
{
    INumberVectorProperty nvp
    {
        "device field",
        "name field",
        "label field",
        "group field",
        IP_RW, 42, IPS_BUSY,
        nullptr, 0,
        "timestamp field",
        nullptr
    };

    // Setting a property
    INDI::Property p(&nvp);
    ASSERT_EQ(p.getType(), INDI_NUMBER);

    // Property fields remain unpropagated
    ASSERT_STREQ(p.getName(), "name field");
    ASSERT_STREQ(p.getLabel(), "label field");
    ASSERT_STREQ(p.getGroupName(), "group field");
    ASSERT_STREQ(p.getDeviceName(), "device field");
    ASSERT_STREQ(p.getTimestamp(), "timestamp field");

    // Other fields remain unchanged
    ASSERT_EQ(p.getRegistered(), true);
    ASSERT_EQ(p.isDynamic(), false);
    ASSERT_EQ(p.getState(), IPS_BUSY);
    ASSERT_EQ(p.getPermission(), IP_RW);
    ASSERT_FALSE(p.getBaseDevice());

    ASSERT_TRUE(p.getNumber());

    // Other specific conversions return invalid property
    ASSERT_FALSE(p.getText());
    ASSERT_FALSE(p.getSwitch());
    ASSERT_FALSE(p.getLight());
    ASSERT_FALSE(p.getBLOB());

    // Clearing a property brings it back to the unregistered state
    p = INDI::Property();
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
    ASSERT_EQ(p.getState(), IPS_ALERT);
    ASSERT_EQ(p.getPermission(), IP_RO);
    ASSERT_FALSE(p.getBaseDevice());

    // Again, conversions return nothing
    ASSERT_FALSE(p.getNumber());
    ASSERT_FALSE(p.getText());
    ASSERT_FALSE(p.getSwitch());
    ASSERT_FALSE(p.getLight());
    ASSERT_FALSE(p.getBLOB());
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
    ASSERT_EQ(p[0].getState(),  IPS_OK);

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
