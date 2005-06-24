/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2005 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
#include <stdint.h>
#include "bacenum.h"
#include "bacdcode.h"
#include "bacdef.h"
#include "rpm.h"

// encode service
int rpm_encode_apdu(
  uint8_t *apdu, 
  uint8_t invoke_id,
  BACNET_READ_PROPERTY_MULTIPLE_DATA **data_list,
  unsigned list_len)
{
  int len = 0; // length of each encoding
  int apdu_len = 0; // total length of the apdu, return value
  unsigned index = 0; // object list index
  unsigned property_index = 0; // property list index
  BACNET_READ_PROPERTY_MULTIPLE_DATA *data;
  BACNET_PROPERTY_REFERENCE *property;

  if (apdu)
  {
    apdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    apdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    apdu[2] = invoke_id; 
    apdu[3] = SERVICE_CONFIRMED_READ_PROPERTY_MULTIPLE;     // service choice
    apdu_len = 4;
    // FIXME: limit the number of ReadAccessSpecification per MAX_APDU
    // list of ReadAccessSpecification
    for (index = 0; index < list_len; index++)
    {    
      data = data_list[index];
      len = encode_context_object_id(&apdu[apdu_len], 0,
        data->object_type, data->object_instance);
      apdu_len += len;
      // sequence of BACnetPropertyReference
      apdu_len += encode_opening_tag(&apdu[apdu_len], 1);
      // loop through list of properties
      for (property_index = 0; 
        property_index < data->property_list_len; 
        property_index++)
      {
        property = data->property_list[property_index];
        len = encode_context_enumerated(&apdu[apdu_len], 0,
          property->object_property);
        apdu_len += len;
        /* optional array index */
        if (property->array_index != BACNET_ARRAY_ALL)
        {  
          len = encode_context_unsigned(&apdu[apdu_len], 1,
            property->array_index);
          apdu_len += len;
        }
      }
      apdu_len += encode_closing_tag(&apdu[apdu_len], 1);
    }
  }
  
  return apdu_len;
}

// decode the service request only
// note: load the data lists with empty data, and set the length
// before calling this function
int rpm_decode_service_request(
  uint8_t *apdu,
  unsigned apdu_len,
  BACNET_READ_PROPERTY_MULTIPLE_DATA **data_list,
  unsigned *data_list_len) 
{
  unsigned len = 0;
  uint8_t tag_number = 0;
  uint32_t len_value_type = 0;
  int type = 0; // for decoding
  int property = 0; // for decoding
  unsigned array_value = 0; // for decoding

  // check for value pointers
  if (apdu_len && data)
  {
    // list of ReadAccessSpecification
    while (len < apdu_len)
    {
      data = data_list[index];
      // Tag 0: Object ID
      if (!decode_is_context_tag(&apdu[len++], 0))
        return -1;
      len += decode_object_id(&apdu[len], &type, &data->object_instance);
      data->object_type = type;
      // Tag 1: sequence of BACnetPropertyReference
      if (decode_is_opening_tag_number(&apdu[len], 1))
      {
        len++; // opening tag is only one octet
        // Tag 0: propertyIdentifier
        len += decode_tag_number_and_value(&apdu[len],
            &tag_number, &len_value_type);
        if (tag_number != 0)
          return -1;
        len += decode_enumerated(&apdu[len], len_value_type, &property);
        data->object_property = property;
        // Tag 1: Optional propertyArrayIndex
        if (len < apdu_len)
        {
          len += decode_tag_number_and_value(&apdu[len],&tag_number,
            &len_value_type);
          if (tag_number == 2)
          {
            len += decode_unsigned(&apdu[len], len_value_type,
              &array_value);
            data->array_index = array_value;
          }
          else
            data->array_index = BACNET_ARRAY_ALL;
        }
        else
          data->array_index = BACNET_ARRAY_ALL;
      }
    }
  }

  return (int)len;
}

int rpm_decode_apdu(
  uint8_t *apdu,
  unsigned apdu_len,
  uint8_t *invoke_id,
  BACNET_READ_PROPERTY_DATA *data)
{
  int len = 0;
  unsigned offset = 0;

  if (!apdu)
    return -1;
  // optional checking - most likely was already done prior to this call
  if (apdu[0] != PDU_TYPE_CONFIRMED_SERVICE_REQUEST)
    return -1;
  //  apdu[1] = encode_max_segs_max_apdu(0, Device_Max_APDU_Length_Accepted());
  *invoke_id = apdu[2]; /* invoke id - filled in by net layer */
  if (apdu[3] != SERVICE_CONFIRMED_READ_PROPERTY)
    return -1;
  offset = 4;

  if (apdu_len > offset)
  {
    len = rpm_decode_service_request(
      &apdu[offset],
      apdu_len - offset,
      data);
  }
  
  return len;
}

int rpm_ack_encode_apdu(
  uint8_t *apdu,
  uint8_t invoke_id,
  BACNET_READ_PROPERTY_DATA *data)
{
  int len = 0; // length of each encoding
  int apdu_len = 0; // total length of the apdu, return value

  if (apdu)
  {
    apdu[0] = PDU_TYPE_COMPLEX_ACK;     /* complex ACK service */
    apdu[1] = invoke_id;        /* original invoke id from request */
    apdu[2] = SERVICE_CONFIRMED_READ_PROPERTY;  // service choice
    apdu_len = 3;
    // service ack follows
    apdu_len += encode_context_object_id(&apdu[apdu_len], 0,
      data->object_type, data->object_instance);
    apdu_len += encode_context_enumerated(&apdu[apdu_len], 1,
      data->object_property);
    // context 2 array index is optional
    if (data->array_index != BACNET_ARRAY_ALL)
    {
      apdu_len += encode_context_unsigned(&apdu[apdu_len], 2,
        data->array_index);
    }
    // propertyValue
    apdu_len += encode_opening_tag(&apdu[apdu_len], 3);
    for (len = 0; len < data->application_data_len; len++)
    {
      apdu[apdu_len++] = data->application_data[len];
    }
    apdu_len += encode_closing_tag(&apdu[apdu_len], 3);
  }

  return apdu_len;
}

int rpm_ack_decode_service_request(
  uint8_t *apdu,
  int apdu_len, // total length of the apdu
  BACNET_READ_PROPERTY_DATA *data)
{
  uint8_t tag_number = 0;
  uint32_t len_value_type = 0;
  int tag_len = 0; // length of tag decode
  int len = 0; // total length of decodes
  int object = 0, property = 0; // for decoding
  unsigned array_value = 0; // for decoding

  // FIXME: check apdu_len against the len during decode  
  // Tag 0: Object ID
  if (!decode_is_context_tag(&apdu[0], 0))
    return -1;
  len = 1;
  len += decode_object_id(&apdu[len],
    &object, &data->object_instance);
  data->object_type = object;
  // Tag 1: Property ID
  len += decode_tag_number_and_value(&apdu[len],
    &tag_number, &len_value_type);
  if (tag_number != 1)
    return -1;
  len += decode_enumerated(&apdu[len],
    len_value_type,
    &property);
  data->object_property = property;
  // Tag 2: Optional Array Index
  tag_len = decode_tag_number_and_value(&apdu[len],
        &tag_number, &len_value_type);
  if (tag_number == 2)
  {
    len += tag_len;
    len += decode_unsigned(&apdu[len],
      len_value_type, &array_value);
    data->array_index = array_value;
  }
  else
    data->array_index = BACNET_ARRAY_ALL;
    
  // Tag 3: opening context tag */
  if (decode_is_opening_tag_number(&apdu[len], 3))
  {
    // a tag number of 3 is not extended so only one octet
    len++;
    // don't decode the application tag number or its data here
    data->application_data = &apdu[len];
    data->application_data_len = apdu_len - len - 1 /*closing tag*/;
  }
  else
    return -1;
  
  return len;
}

int rpm_ack_decode_apdu(
  uint8_t *apdu,
  int apdu_len, // total length of the apdu
  uint8_t *invoke_id,
  BACNET_READ_PROPERTY_DATA *data)
{
  int len = 0;
  int offset = 0;

  if (!apdu)
    return -1;
  // optional checking - most likely was already done prior to this call
  if (apdu[0] != PDU_TYPE_COMPLEX_ACK)
    return -1;
  *invoke_id = apdu[1];
  if (apdu[2] != SERVICE_CONFIRMED_READ_PROPERTY)
    return -1;
  offset = 3;
  if (apdu_len > offset)
  {
    len = rpm_ack_decode_service_request(
      &apdu[offset],
      apdu_len - offset,
      data);
  }
  
  return len;
}

#ifdef TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

void testReadPropertyAck(Test * pTest)
{
  uint8_t apdu[480] = {0};
  uint8_t apdu2[480] = {0};
  int len = 0;
  int apdu_len = 0;
  uint8_t invoke_id = 1;
  uint8_t test_invoke_id = 0;
  BACNET_READ_PROPERTY_DATA data;
  BACNET_READ_PROPERTY_DATA test_data;
  BACNET_OBJECT_TYPE object_type = OBJECT_DEVICE;
  uint32_t object_instance = 0;
  int object = 0;

  data.object_type = OBJECT_DEVICE;
  data.object_instance = 1;
  data.object_property = PROP_OBJECT_IDENTIFIER;
  data.array_index = BACNET_ARRAY_ALL;

  data.application_data_len = encode_bacnet_object_id(&apdu2[0], 
    data.object_type, 
    data.object_instance);
  data.application_data = &apdu2[0];
  
  len = rpm_ack_encode_apdu(
    &apdu[0],
    invoke_id,
    &data);
  ct_test(pTest, len != 0);
  ct_test(pTest, len != -1);
  apdu_len = len;
  len = rpm_ack_decode_apdu(
    &apdu[0],
    apdu_len, // total length of the apdu
    &test_invoke_id,
    &test_data);
  ct_test(pTest, len != -1);
  ct_test(pTest, test_invoke_id == invoke_id);

  ct_test(pTest, test_data.object_type == data.object_type);
  ct_test(pTest, test_data.object_instance == data.object_instance);
  ct_test(pTest, test_data.object_property == data.object_property);
  ct_test(pTest, test_data.array_index == data.array_index);
  ct_test(pTest, test_data.application_data_len == data.application_data_len);

  len = decode_object_id(
    test_data.application_data, 
    &object, 
    &object_instance);
  object_type = object;
  ct_test(pTest, object_type == data.object_type);
  ct_test(pTest, object_instance == data.object_instance);
}

void testReadProperty(Test * pTest)
{
  uint8_t apdu[480] = {0};
  int len = 0;
  int apdu_len = 0;
  uint8_t invoke_id = 128;
  uint8_t test_invoke_id = 0;
  BACNET_READ_PROPERTY_DATA data;
  BACNET_READ_PROPERTY_DATA test_data;
      
  data.object_type = OBJECT_DEVICE;
  data.object_instance = 1;
  data.object_property = PROP_OBJECT_IDENTIFIER;
  data.array_index = BACNET_ARRAY_ALL;
  len = rpm_encode_apdu(
    &apdu[0],
    invoke_id,
    &data);
  ct_test(pTest, len != 0);
  apdu_len = len;

  len = rpm_decode_apdu(
    &apdu[0],
    apdu_len,
    &test_invoke_id,
    &test_data);
  ct_test(pTest, len != -1);
  ct_test(pTest, test_data.object_type == data.object_type);
  ct_test(pTest, test_data.object_instance == data.object_instance);
  ct_test(pTest, test_data.object_property == data.object_property);
  ct_test(pTest, test_data.array_index == data.array_index);

  return;
}

#ifdef TEST_READ_PROPERTY
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet ReadProperty", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testReadProperty);
    assert(rc);
    rc = ct_addTestFunction(pTest, testReadPropertyAck);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif                          /* TEST_READ_PROPERTY */
#endif                          /* TEST */
