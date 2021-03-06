<?php

function _cnapi_parse_error($xml) {
  $data = array();

  $xml = str_replace(' xmlns=', ' ns=', $xml);

  // trying to parse the xml
  try {
    $xml = new SimpleXMLElement($xml);
  }
  catch (Exception $e) {
    watchdog('cnapi', 'Could not parse error !xml.', array('!xml' => $xml), WATCHDOG_ERROR);
    return FALSE;
  }

  _cnapi_parse_str($data, 'code', $xml, '/cdbxml/response/code');
  _cnapi_parse_str($data, 'level', $xml, '/cdbxml/response/level');
  _cnapi_parse_str($data, 'message', $xml, '/cdbxml/response/message');
  _cnapi_parse_str($data, 'resource', $xml, '/cdbxml/response/resource');

  // return if we could succesfully parse fields
  if (!empty($data)) {
    return $data;
  }

  // return FALSE in other cases
  return FALSE;
}

function _cnapi_parse($request, $xml) {
  $data = array();

  // we don't care for namespaces here
  $xml = str_replace(' xmlns=', ' ns=', $xml);

  // trying to parse the xml
  try {
    $xml = new SimpleXMLElement($xml);
  }
  catch (Exception $e) {
    watchdog('cnapi', 'Could not parse request result for request !request.', array('!request' => serialize($request)), WATCHDOG_ERROR);
    return FALSE;
  }

  // detail
  if ($request['action'] == 'detail') {
    $xpath = sprintf('/cdbxml/%ss/%s', $request['type'], $request['type']);
    if ($xml_object = $xml->xpath($xpath)) {
      switch ($request['type']) {
        case 'actor':
          _cnapi_parse_actor($data, NULL, reset($xml_object));
          break;
        case 'event':
          _cnapi_parse_event($data, NULL, reset($xml_object));
          break;
        case 'production':
          _cnapi_parse_production($data, NULL, reset($xml_object));
          break;
      }
    }
  }

  // list_detail
  elseif ($request['action'] == 'list_detail') {
    // $data['total']
    $data['total'] = _cnapi_xpath_str($xml, 'nofrecords');

    // $data['data']
    $data['data'] = array();

    $xpath = sprintf('/cdbxml/%ss/%s', $request['type'], $request['type']);
    if ($xml_objects = $xml->xpath($xpath)) {
      foreach ($xml_objects as $xml_object) {
        $object = array();
        switch ($request['type']) {
          case 'actor':
            _cnapi_parse_actor($object, NULL, $xml_object);
            break;
          case 'event':
            _cnapi_parse_event($object, NULL, $xml_object);
            break;
          case 'production':
            _cnapi_parse_production($object, NULL, $xml_object);
            break;
        }
        if (!empty($object)) {
          $data['data'][] = $object;
        }
      }
    }

    // $data['deletes']
    $xpath = '/cdbxml/deletes/delete';
    if ($xml_objects = $xml->xpath($xpath)) {
      $data['deletes'] = array();
      foreach ($xml_objects as $xml_object) {
        $cdbid = _cnapi_xpath_str($xml_object, '@cdbid');
        if (!empty($cdbid)) {
          $data['deletes'][] = $cdbid;
        }
      }
    }
  }

  // list_summary
  elseif ($request['action'] == 'list_summary') {
    // $data['total']
    $data['total'] = _cnapi_xpath_str($xml, 'nofrecords');

    // $data['data']
    $items_path = '/cdbxml/list/item';
    if ($request['type'] == 'production') {
      $items_path = '/cdbxml/production/item';
    }

    $xml_items = $xml->xpath($items_path);
    foreach ($xml_items as $xml_item) {
      $object = array();
      switch ($request['type']) {
        case 'actor':
          _cnapi_parse_actor_list_item($object, $xml_item);
          break;
        case 'event':
          _cnapi_parse_event_list_item($object, $xml_item);
          break;
        case 'production':
          _cnapi_parse_production_list_item($object, $xml_item);
          break;
      }

      $data['data'][] = $object;
    }
  }

  elseif ($request['action'] == 'report') {
    // parsing all report types
    $types = array('headings', 'time', 'age', 'production', 'region', 'location');
    
    if (isset($request['options']['parse_geo']) && $request['options']['parse_geo']) {
      $types[] = 'geo';
    }
    
    foreach ($types as $type) {
      $xml_items = $xml->xpath('report/' . $type . '/item');
      foreach ($xml_items as $xml_item) {
        $item = array();
        _cnapi_parse_report_item($item, $xml_item);
        if (!empty($item)) {
          if ($type != 'time') {
            $data[$type][$item['id']] = $item;
          }
          else {
            $data[$type][$item['name']] = $item;
          }
        }
      }
    }

    // in case of a "time" or "age" report, id should be the name
    $types = array('time', 'age');
    foreach ($types as $type) {
      if (!empty($data[$type])) {
        foreach ($data[$type] as $i => $item) {
          $data[$type][$i]['id'] = $data[$type][$i]['name'];
          if ($type == 'time') {
            $data[$type][$i]['name'] = cnapi_get_datetype($data[$type][$i]['id']);
          }
        }
      }
    }
  }

  return $data;
}

function _cnapi_parse_report_item(&$data, $xml) {
  $attrs = $xml->attributes();
  
  // $data['name']
  $data['name'] = strval($attrs['name']);

  // $data['id']
  $data['id'] = strval($attrs['id']);

  // $data['total']
  $data['total'] = strval($attrs['total']);

  // $data['link']
  $data['link'] = strval($attrs['link']);
  if (!empty($data['link'])) {
    $data['link'] = cnapi_url_a2p($data['link']);
    cnapi_request_remove_defaults($data['link']);
  }

  // $data['children']
  foreach ($xml->item as $xml_child) {
    $child = array();
    _cnapi_parse_report_item($child, $xml_child);
    if (!empty($child)) {
      $data['children'][$child['id']] = $child;
    }
  }
}

function _cnapi_parse_event_list_item(&$data, $xml) {
  // $data['cdbid']
  _cnapi_parse_str($data, 'cdbid', $xml, '@cdbid');

  // $data['title']
  _cnapi_parse_str($data, 'title', $xml, '@title');

  // $data['shortdescription']
  _cnapi_parse_str($data, 'shortdescription', $xml, '@shortdescription');
  _cnapi_parse_shortdescription($data, 'shortdescription');

  // $data['thumbnail']
  _cnapi_parse_str($data, 'thumbnail', $xml, '@thumbnail');

  // $data['city']
  _cnapi_parse_str($data, 'city', $xml, '@city');

  // $data['zip']
  _cnapi_parse_str($data, 'zip', $xml, '@zip');

  // $data['cityid']
  if (isset($data['zip']) && isset($data['city'])) {
    $data['cityid'] = cnapi_get_cityid($data['zip'], $data['city']);
  }

  // $data['address']
  _cnapi_parse_str($data, 'address', $xml, '@address');
  if (!empty($attrs['latlng'])) {
    $latlng = explode(';', strval($attrs['latlng']));
    if (count($latlng) == 2) {
      $data['gis']['lat'] = $latlng[0];
      $data['gis']['lng'] = $latlng[1];
    }
  }

  // $data['calendarsummary']
  _cnapi_parse_str($data, 'calendarsummary', $xml, '@calendarsummary');

  // $data['headingid']
  _cnapi_parse_str($data, 'headingid', $xml, '@headingid');
  if (isset($data['headingid'])) {
    $data['headingid'] = explode(';', $data['headingid']);
  }

  // $data['locationid']
  _cnapi_parse_str($data, 'locationid', $xml, '@locationid');

  // $data['location']
  _cnapi_parse_str($data, 'location', $xml, '@location');

  // $data['available_to']
  _cnapi_parse_str($data, 'available_to', $xml, '@available_to');
  if (isset($data['available_to'])) {
    $data['available_to'] = cnapi_timestamp($data['available_to']);
  }

  // $data['agefrom']
  _cnapi_parse_str($data, 'agefrom', $xml, '@agefrom');

  // $data['performers']
  _cnapi_parse_str($data, 'performers', $xml, '@performers');
  if (isset($data['performers'])) {
    $data['performers'] = explode(';', $data['performers']);
  }

  // $data['created']
  _cnapi_parse_str($data, 'created', $xml, '@created');
  if (isset($data['created'])) {
    $data['created'] = cnapi_timestamp($data['created']);
  }
  
  // $data['reservation']
  _cnapi_parse_str($data, 'reservation', $xml, '@reservation');
}

function _cnapi_parse_production_list_item(&$data, $xml) {
  // $data['cdbid']
  _cnapi_parse_str($data, 'cdbid', $xml, '@cdbid');

  // $data['title']
  _cnapi_parse_str($data, 'title', $xml, '@title');

  // $data['shortdescription']
  _cnapi_parse_str($data, 'shortdescription', $xml, '@shortdescription');
  _cnapi_parse_shortdescription($data, 'shortdescription');

  // $data['thumbnail']
  _cnapi_parse_str($data, 'thumbnail', $xml, '@thumbnail');

  // $data['agefrom']
  _cnapi_parse_str($data, 'agefrom', $xml, '@agefrom');

  // $data['performers']
  _cnapi_parse_str($data, 'performers', $xml, '@performers');
  if (isset($data['performers'])) {
    $data['performers'] = explode(';', $data['performers']);
  }

  // $data['created']
  _cnapi_parse_str($data, 'created', $xml, '@created');
  if (isset($data['created'])) {
    $data['created'] = cnapi_timestamp($data['created']);
  }
}

function _cnapi_parse_actor_list_item(&$data, $xml) {
  // $data['cdbid']
  _cnapi_parse_str($data, 'cdbid', $xml, '@cdbid');

  // $data['title']
  _cnapi_parse_str($data, 'title', $xml, '@title');

  // $data['shortdescription']
  _cnapi_parse_str($data, 'shortdescription', $xml, '@shortdescription');
  _cnapi_parse_shortdescription($data, 'shortdescription');

  // $data['thumbnail']
  _cnapi_parse_str($data, 'thumbnail', $xml, '@thumbnail');

  // $data['city']
  _cnapi_parse_str($data, 'city', $xml, '@city');

  // $data['zip']
  _cnapi_parse_str($data, 'zip', $xml, '@zip');

  // $data['citid']
  if (isset($data['zip']) && isset($data['city'])) {
    $data['cityid'] = cnapi_get_cityid($data['zip'], $data['city']);
  }

  // $data['address']
  _cnapi_parse_str($data, 'address', $xml, '@address');
  $latlng = explode(';', _cnapi_xpath_str($xml, '@latlng'));
  if (count($latlng) == 2) {
    $data['gis']['lat'] = $latlng[0];
    $data['gis']['lng'] = $latlng[1];
  }
}

function _cnapi_parse_event(&$data, $field = NULL, $xml) {
  $event = array();
  
  // $event['type']
  $event['type'] = 'event';

  // $event['cdbid']
  _cnapi_parse_str($event, 'cdbid', $xml, '@cdbid');

  // $event['detail'][...]
  _cnapi_parse_event_details($event, 'detail', $xml);

  // $event['categories'][...]
  _cnapi_parse_categories($event, 'categories', $xml);

  // $event['keywords']
  _cnapi_parse_str($event, 'keywords', $xml, 'keywords');
  if (isset($event['keywords'])) {
    $event['keywords'] = explode(';', $event['keywords']);
  }

  // $event['headings']
  _cnapi_parse_str($event, 'headings', $xml, 'headings/heading/@id', TRUE);

  // $event['agefrom']
  _cnapi_parse_str($event, 'agefrom', $xml, 'agefrom');

  // $event['calendar'][...]
  _cnapi_parse_calendar($event, 'calendar', $xml);

  // $event['contactinfo'][...]
  _cnapi_parse_contactinfo($event, 'contactinfo', $xml);

  // $event['location'][...]
  _cnapi_parse_location($event, 'location', $xml);

  // $event['organiser'][...]
  if ($xml_organiser = $xml->xpath('organiser/actor')) {
    _cnapi_parse_actor($event, 'organiser', reset($xml_organiser));
  }

  // $event['languages'][...]
  _cnapi_parse_languages($event, 'languages', $xml);

  // $event['related_events'][...]
  _cnapi_parse_related_events($event, 'related_events', $xml);

  if ($xml_event_relations = $xml->xpath('eventrelations')) {
    $xml_event_relations = reset($xml_event_relations);

    // $event['related_production'][...]
    _cnapi_parse_related_production($event, 'related_production', $xml_event_relations);

    // $event['parent_event'][...]
    _cnapi_parse_parent_event($event, 'parent_event', $xml_event_relations);
  }

  if (!empty($event)) {
    // adding data to requesting object
    if ($field) {
      $data[$field] = $event;
    }
    else {
      $data = $event;
    }
  }
}

function _cnapi_parse_production(&$data, $field = NULL, $xml) {
  $production = array();
  
  // $production['type']
  $production['type'] = 'production';

  // $production['cdbid']
  _cnapi_parse_str($production, 'cdbid', $xml, '@cdbid');

  // $production['detail'][...]
  _cnapi_parse_production_details($production, 'detail', $xml);

  // $production['categories'][...]
  _cnapi_parse_categories($production, 'categories', $xml);

  // $production['keywords']
  _cnapi_parse_str($production, 'keywords', $xml, 'keywords');
  if (isset($production['keywords'])) {
    $production['keywords'] = explode(';', $production['keywords']);
  }

  // $production['agefrom']
  _cnapi_parse_str($production, 'agefrom', $xml, 'agefrom');

  // $production['related_events'][...]
  _cnapi_parse_related_events($production, 'related_events', $xml);

  if (!empty($production)) {
    // adding data to requesting object
    if ($field) {
      $data[$field] = $production;
    }
    else {
      $data = $production;
    }
  }
}

function _cnapi_parse_actor(&$data, $field = NULL, $xml) {
  $actor = array();
  
  // $actor['type']
  $actor['type'] = 'actor';

  // $actor['cdbid']
  _cnapi_parse_str($actor, 'cdbid', $xml, '@cdbid');

  // $actor['asset']
  _cnapi_parse_bool($actor, 'asset', $xml, '@asset');

  // $actor['detail'][...]
  _cnapi_parse_actor_details($actor, 'detail', $xml);

  // $actor['categories'][...]
  _cnapi_parse_categories($actor, 'categories', $xml);

  // $actor['keywords']
  _cnapi_parse_str($actor, 'keywords', $xml, 'keywords');
  if (isset($actor['keywords'])) {
    $actor['keywords'] = explode(';', $actor['keywords']);
  }

  // $actor['contactinfo'][...]
  _cnapi_parse_contactinfo($actor, 'contactinfo', $xml);

  if (!empty($actor)) {
    // adding data to requesting object
    if ($field) {
      $data[$field] = $actor;
    }
    else {
      $data = $actor;
    }
  }
}

function _cnapi_parse_event_details(&$data, $field, $xml) {
  $details = array();

  if ($languages = _cnapi_xpath_str($xml, 'eventdetails/eventdetail/@lang', TRUE)) {
    foreach ($languages as $language) {
      $xml_detail = reset($xml->xpath('eventdetails/eventdetail[@lang="' . $language . '"]'));
      _cnapi_parse_event_detail($details, $language, $xml_detail);
    }
  }

  // adding data to requesting object
  if (!empty($details)) {
    $data[$field] = $details;
  }
}

function _cnapi_parse_event_detail(&$data, $field, $xml) {
  $detail = array();

  // $detail['title']
  _cnapi_parse_str($detail, 'title', $xml, 'title');

  // $detail['shortdescription']
  _cnapi_parse_str($detail, 'shortdescription', $xml, 'shortdescription');
  _cnapi_parse_shortdescription($detail, 'shortdescription');

  // $detail['longdescription']
  _cnapi_parse_str($detail, 'longdescription', $xml, 'longdescription');

  // $detail['calendarsummary']
  _cnapi_parse_str($detail, 'calendarsummary', $xml, 'calendarsummary');

  // $detail['performers'][...]
  _cnapi_parse_performers($detail, 'performers', $xml);

  // $detail['media'][...]
  _cnapi_parse_media($detail, 'media', $xml);

  // $detail['price'][...]
  _cnapi_parse_price($detail, 'price', $xml);

  // adding data to requesting object
  if (!empty($detail)) {
    $data[$field] = $detail;
  }
}

function _cnapi_parse_production_details(&$data, $field, $xml) {
  $details = array();

  if ($languages = _cnapi_xpath_str($xml, 'productiondetails/productiondetail/@lang', TRUE)) {
    foreach ($languages as $language) {
      $xml_detail = reset($xml->xpath('productiondetails/productiondetail[@lang="' . $language . '"]'));
      _cnapi_parse_production_detail($details, $language, $xml_detail);
    }
  }

  // adding data to requesting object
  if (!empty($details)) {
    $data[$field] = $details;
  }
}

function _cnapi_parse_production_detail(&$data, $field, $xml) {
  $detail = array();

  // $detail['title']
  _cnapi_parse_str($detail, 'title', $xml, 'title');

  // $detail['shortdescription']
  _cnapi_parse_str($detail, 'shortdescription', $xml, 'shortdescription');
  _cnapi_parse_shortdescription($detail, 'shortdescription');

  // $detail['longdescription']
  _cnapi_parse_str($detail, 'longdescription', $xml, 'longdescription');

  // $detail['performers'][...]
  _cnapi_parse_performers($detail, 'performers', $xml);

  // $detail['media'][...]
  _cnapi_parse_media($detail, 'media', $xml);

  // adding data to requesting object
  if (!empty($detail)) {
    $data[$field] = $detail;
  }
}

function _cnapi_parse_actor_details(&$data, $field, $xml) {
  $details = array();

  if ($languages = _cnapi_xpath_str($xml, 'actordetails/actordetail/@lang', TRUE)) {
    foreach ($languages as $language) {
      $xml_detail = reset($xml->xpath('actordetails/actordetail[@lang="' . $language . '"]'));
      _cnapi_parse_actor_detail($details, $language, $xml_detail);
    }
  }

  // adding data to requesting object
  if (!empty($details)) {
    $data[$field] = $details;
  }
}

function _cnapi_parse_actor_detail(&$data, $field, $xml) {
  $detail = array();

  // $detail['title']
  _cnapi_parse_str($detail, 'title', $xml, 'title');

  // $detail['shortdescription']
  _cnapi_parse_str($detail, 'shortdescription', $xml, 'shortdescription');
  _cnapi_parse_shortdescription($detail, 'shortdescription');

  // $detail['longdescription']
  _cnapi_parse_str($detail, 'longdescription', $xml, 'longdescription');

  // $detail['media'][...]
  _cnapi_parse_media($detail, 'media', $xml);

  // $detail['calendarsummary']
  _cnapi_parse_str($detail, 'calendarsummary', $xml, 'calendarsummary');

  // adding data to requesting object
  if (!empty($detail)) {
    $data[$field] = $detail;
  }
}

function _cnapi_parse_related_production(&$data, $field, $xml) {
  // $data[$field]['cdbid']
  _cnapi_parse_str($data[$field], 'cdbid', $xml, 'relatedproduction/@cdbid');

  // $data[$field]['title']
  _cnapi_parse_str($data[$field], 'title', $xml, 'relatedproduction');

  // removing empty related_production
  if (empty($data[$field])) {
    unset($data[$field]);
  }
}

function _cnapi_parse_parent_event(&$data, $field, $xml) {
  // $data[$field]['cdbid']
  _cnapi_parse_str($data[$field], 'cdbid', $xml, 'parentevent/@cdbid');

  // $data[$field]['title']
  _cnapi_parse_str($data[$field], 'title', $xml, 'parentevent');

  // removing empty parent_event
  if (empty($data[$field])) {
    unset($data[$field]);
  }
}

function _cnapi_parse_related_events(&$data, $field, $xml) {
  $related_events = array();

  // related events as list_summary
  if ($items = $xml->xpath('relatedevents/list/item')) {
    foreach ($items as $item) {
      $object = array();
      _cnapi_parse_event_list_item($object, $item);
      if (!empty($object)) {
        $related_events[] = $object;
      }
    }
  }

  // related events as list_detail
  elseif ($items = $xml->xpath('relatedevents/event')) {
    foreach ($items as $item) {
      $object = array();
      _cnapi_parse_event($object, NULL, $item);
      if (!empty($object)) {
        $related_events[] = $object;
      }
    }
  }

  // related events as list of cdbid
  else {
    $related_events = _cnapi_xpath_str($xml, 'relatedevents/id/@cdbid', TRUE);
  }

  // adding data to requesting object
  if (!empty($related_events)) {
    $data[$field] = $related_events;
  }
}

function _cnapi_parse_calendar(&$data, $field, $xml) {  // calendar
  $timestamps = $xml->xpath('calendar/timestamps/timestamp');
  $periods = $xml->xpath('calendar/periods/period');
  
  if ($timestamps) {
    $data[$field]['start'] = strtotime(strval(reset($timestamps[0]->xpath('date'))) . ' ' . strval(reset($timestamps[0]->xpath('timestart'))));
    if (count($timestamps) > 1) {
      $last = count($timestamps) - 1;
    }
    else {
      $last = 0;
    }
    $data[$field]['stop'] = strtotime(strval(reset($timestamps[$last]->xpath('date'))) . ' ' . strval(reset($timestamps[$last]->xpath('timestart'))));
  }
  elseif ($periods) {
    $data[$field]['start'] = strtotime(reset($periods[0]->xpath('datefrom')));
    $data[$field]['stop'] = strtotime(reset($periods[0]->xpath('dateto')));
  }
}

function _cnapi_parse_languages(&$data, $field, $xml) {
  $languages = array();

  // related events as list_summary
  if ($items = $xml->xpath('languages/language')) {
    foreach ($items as $item) {
      $language = array();
      _cnapi_parse_str($language, 'name', $item, '.');
      _cnapi_parse_str($language, 'type', $item, '@type');
      if (!empty($language['name'])) {
        $languages[] = $language;
      }
    }
  }

  // adding data to requesting object
  if (!empty($languages)) {
    $data[$field] = $languages;
  }
}

function _cnapi_parse_contactinfo(&$data, $field, $xml) {
  $info = array(
    'main' => array(),
    'reservation' => array(),
    'other' => array(),
  );

  // $info['address'][...]
  $contactinfo_xml = $xml->xpath('contactinfo');

  if (!$contactinfo_xml) {
    return;
  }

  _cnapi_parse_address($info, 'address', $contactinfo_xml[0]);;

  // $info['main']['mail']
  _cnapi_parse_str($info['main'], 'mail', $xml, 'contactinfo/mail[@main="true"]');

  // $info['main']['phone']
  _cnapi_parse_str($info['main'], 'phone', $xml, 'contactinfo/phone[@main="true" and @type="phone"]');

  // $info['main']['fax']
  _cnapi_parse_str($info['main'], 'fax', $xml, 'contactinfo/phone[@main="true" and @type="fax"]');

  // $info['main']['url']
  _cnapi_parse_str($info['main'], 'url', $xml, 'contactinfo/url[@main="true"]');

  // $info['reservation']['mail']
  _cnapi_parse_str($info['reservation'], 'mail', $xml, 'contactinfo/mail[@reservation="true"]');

  // $info['reservation']['phone']
  _cnapi_parse_str($info['reservation'], 'phone', $xml, 'contactinfo/phone[@reservation="true" and @type="phone"]');

  // $info['reservation']['fax']
  _cnapi_parse_str($info['reservation'], 'fax', $xml, 'contactinfo/phone[@reservation="true" and @type="fax"]');

  // $info['reservation']['url']
  _cnapi_parse_str($info['reservation'], 'url', $xml, 'contactinfo/url[@reservation="true"]');

  // $info['other']['mail']
  _cnapi_parse_str($info['other'], 'mail', $xml, 'contactinfo/mail[not(@main="true" or @reservation="true")]', TRUE);

  // $info['other']['phone']
  _cnapi_parse_str($info['other'], 'phone', $xml, 'contactinfo/phone[not(@main="true" or @reservation="true") and @type="phone"]', TRUE);

  // $info['other']['fax']
  _cnapi_parse_str($info['other'], 'fax', $xml, 'contactinfo/phone[not(@main="true" or @reservation="true") and @type="fax"]', TRUE);

  // $info['other']['url']
  _cnapi_parse_str($info['other'], 'url', $xml, 'contactinfo/url[not(@main="true" or @reservation="true")]', TRUE);

  // removing empty contactinfo types
  foreach ($info as $type => $value) {
    if (empty($info[$type])) {
      unset($info[$type]);
    }
  }

  // adding data to requesting object
  if (!empty($info)) {
    $data[$field] = $info;
  }
}

function _cnapi_parse_categories(&$data, $field, $xml) {
  $categories = array();

  if ($types = _cnapi_xpath_str($xml, 'categories/category/@type', TRUE)) {
    foreach ($types as $type) {
      _cnapi_parse_str($categories, $type, $xml, 'categories/category[@type="' . $type . '"]/@catid', TRUE);
    }
  }

  // adding data to requesting object
  if (!empty($categories)) {
    $data[$field] = $categories;
  }
}

function _cnapi_parse_address(&$data, $field, $xml) {
  $address = array();

  // $address['street']
  _cnapi_parse_str($address, 'street', $xml, 'address/physical/street');

  // $address['housenr']
  _cnapi_parse_str($address, 'housenr', $xml, 'address/physical/housenr');

  // $address['zipcode']
  _cnapi_parse_str($address, 'zipcode', $xml, 'address/physical/zipcode');

  // $address['city']
  _cnapi_parse_str($address, 'city', $xml, 'address/physical/city');

  // $address['country']
  _cnapi_parse_str($address, 'country', $xml, 'address/physical/country');

  // $address['gis'][...]
  $gis = array();
  _cnapi_parse_str($gis, 'lat', $xml, 'address/physical/gis/ycoordinate');
  _cnapi_parse_str($gis, 'lng', $xml, 'address/physical/gis/xcoordinate');
  if (!empty($gis)) {
    $address['gis'] = $gis;
  }

  // $address['cityid']
  if (isset($address['zipcode']) && isset($address['city'])) {
    $address['cityid'] = cnapi_get_cityid($address['zipcode'], $address['city']);
  }

  // adding data to requesting object
  if (!empty($address)) {
    $data[$field] = $address;
  }
}

function _cnapi_parse_location(&$data, $field = NULL, $xml) {
  $location = array();

  // $location['address'][...]
  if ($xml_location = $xml->xpath('location')) {
    $xml_location = reset($xml_location);
    _cnapi_parse_str($location, 'location', $xml_location, 'label');
    _cnapi_parse_address($location, 'address', $xml_location);
  }

  // $location['actor'][...]
  if ($xml_location_actor = $xml->xpath('location/actor')) {
    _cnapi_parse_actor($location, 'actor', reset($xml_location_actor));
  }

  // adding data to requesting object
  if (!empty($location)) {
    $data[$field] = $location;
  }
}

function _cnapi_parse_performers(&$data, $field, $xml) {
  $performers = array();

  if ($xml_performers = $xml->xpath('performers/performer')) {
    foreach ($xml_performers as $xml_performer) {
      // performer as an actor
      if ($xml_actor = $xml_performer->xpath('actor')) {
        // parsing the actor
        $actor = array();
        _cnapi_parse_actor($actor, NULL, reset($xml_actor));

        // if we succesfully parsed an actor, add extra info and add it to list
        if (!empty($actor)) {
          $performer = array();

          // $perfomer['role']
          _cnapi_parse_str($performer, 'role', $xml_performer, 'role');

          // $perfomer['name']
          $performer['name'] = $actor['detail']['nl']['title'];

          // $perfomer['actor']
          $performer['actor'] = $actor;

          // adding the performer
          $performers[] = $performer;
        }
      }

      // peformer as a label
      elseif ($xml_performer->xpath('label')) {
        $performer = array();

        // $perfomer['role']
        _cnapi_parse_str($performer, 'role', $xml_performer, 'role');

        // $perfomer['name']
        _cnapi_parse_str($performer, 'name', $xml_performer, 'label');

        // only adding performer if at least one field is available
        if (!empty($performer)) {
          $performers[] = $performer;
        }
      }
    }
  }

  // adding data to requesting object
  if (!empty($performers)) {
    $data[$field] = $performers;
  }
}

function _cnapi_parse_media(&$data, $field, $xml) {
  $files = array();
  $main_file = array();

  if ($xml_files = $xml->xpath('media/file')) {
    foreach ($xml_files as $xml_file) {
      $file = array();

      // $file['copyright']
      _cnapi_parse_str($file, 'copyright', $xml_file, 'copyright');

      // $file['filename']
      _cnapi_parse_str($file, 'filename', $xml_file, 'filename');

      // $file['filetype']
      _cnapi_parse_str($file, 'filetype', $xml_file, 'filetype');

      // $file['mediatype']
      _cnapi_parse_str($file, 'mediatype', $xml_file, 'mediatype');

      // $file['title']
      _cnapi_parse_str($file, 'title', $xml_file, 'title');

      // $file['hlink']
      _cnapi_parse_str($file, 'hlink', $xml_file, 'hlink');

      // $file['plaintext']
      _cnapi_parse_str($file, 'plaintext', $xml_file, 'plaintext');

      // only adding the file if we have at least one field
      if (!empty($file)) {
        if (_cnapi_xpath_bool($xml_file, '@main')) {
          $main_file = $file;
        }
        else {
          $files[] = $file;
        }
      }
    }
  }

  // adding mail file to requesting object
  if (!empty($main_file)) {
    $data[$field]['main'] = $main_file;
  }

  // adding other files to requesting object
  if (!empty($files)) {
    $data[$field]['other'] = $files;
  }
}

function _cnapi_parse_price(&$data, $field, $xml) {
  $price = array();

  // $price['value']
  _cnapi_parse_float($price, 'value', $xml, 'price/pricevalue');

  // $data['description']
  _cnapi_parse_str($price, 'description', $xml, 'price/pricedescription');

  // adding data to requesting object
  if (!empty($price)) {
    $data[$field] = $price;
  }
}

function _cnapi_parse_shortdescription(&$data, $field) {
  $clean = array(
    'Beschrijving niet beschikbaar',
    'Beschrijving niet beschikbaar.',
    'Niet beschikbaar',
    'NB',
  );

  if (isset($data[$field]) && in_array($data[$field], $clean)) {
    $data[$field] = '';
  }
}