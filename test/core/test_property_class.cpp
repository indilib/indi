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

#include "indiproperty.h"

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

    INumberVectorProperty nvp {
        "device field",
        "name field",
        "label field",
        "group field",
        IP_RW, 42, IPS_BUSY,
        nullptr, 0,
        "timestamp field",
        nullptr };

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
    INDI::BaseDevice * corrupted_device = (INDI::BaseDevice*) (void*) 0x87654321;

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
