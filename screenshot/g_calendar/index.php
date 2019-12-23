<?php
ini_set('display_errors', true);
// Reference here the composer autolad with your libraries
require __DIR__ . './../vendor/autoload.php';

// IMPORTANT: Place this out of the public website!!! 
define("OAUTH_CREDENTIALS", "/home/myuser/credentials.json");
// Important because if you forget about it and this file is compromised someone can read your private calendar
// If you are behind a proxy search put this to true and set up your IP:Port
define("USE_PROXY", false);
define("PROXY", "IP:Port");
define("DATEFORMAT", "d.m.Y");
define("HOURMIN", "H:i");

// Get the API client and construct the service object.
$client = getClient();
$service = new Google_Service_Calendar($client);

// Print the next 10 events on the user's calendar.
$calendarId = 'primary';
$optParams = array(
    'maxResults' => 3,
    'orderBy' => 'startTime',
    'singleEvents' => true,
    'timeMin' => date('c'),
);
$results = $service->events->listEvents($calendarId, $optParams);
$events = $results->getItems();

// Read templates to customize design outside PHP
$readBaseTemplate = file_get_contents('template/template.html');
$readEventRow = file_get_contents('template/partial_event_row.html');

$eventRow = "";
$icon['arrow'] = ' &nbsp;<img src="../arrow-right.svg" width="25">&nbsp; ';
foreach ($events as $event) {
    $dateStart = ($event->start->getDate() != '') ? $event->start->getDate() : $event->start->getDateTime(); 
    $dateEnd = ($event->end->getDate() != '') ? $event->end->getDate() : $event->end->getDateTime(); 
    $start = new DateTime($dateStart);
    $end = new DateTime($dateEnd);
    // TODO: Full day should be handled differently (Shows always next day)	
    if ($event->start->getDate() != '' && $event->end->getDate() != '') {
      $end->modify('-1 days');
    } 
    $startTime = ($start->format(HOURMIN)!='00:00') ? $start->format(HOURMIN) : '';   
    $endTime = ($end->format(HOURMIN)!='00:00') ? $end->format(HOURMIN) : '';   
    $endFormat = ($start->format(DATEFORMAT) != $end->format(DATEFORMAT)) ? 
             $end->format(DATEFORMAT).' '.$endTime : $endTime;
    $startFormat = $start->format(DATEFORMAT).' '.$startTime;
    $fromTo = ($endFormat=='') ? $startFormat : $startFormat.$icon['arrow']. $endFormat;
    $body = str_replace("{{title}}", $event->summary, $readEventRow);
    $body = str_replace("{{from_to}}", $fromTo, $body);

    if (property_exists($event, 'attendees')) {
        $attendees = "";
        foreach ($event->attendees as $attendee) {
	    $attendeeName = explode("@", $attendee->email);	
            $attendees.= '<li>'.$attendeeName[0].'</li>';
        }
        $body = str_replace("{{attendees}}", $attendees, $body);

    } else {
        $body = str_replace("{{attendees}}", '', $body);
    }

    if (property_exists($event, 'location')) {
        $body = str_replace("{{location}}", $event->location, $body);
    } else {
        $body = str_replace("{{location}}", "", $body);
    }

    $eventRow .= $body;
}

// Render all event rows on base template
$container = str_replace("{{event_row}}", $eventRow, $readBaseTemplate);
echo $container;

// DONE!




/**
 * @return Google_Client
 * @throws Exception
 */
function getClient()
{
    $client = new Google_Client();
    $client->setApplicationName('Google Calendar API PHP Quickstart');
    $client->setScopes(Google_Service_Calendar::CALENDAR_READONLY);
    $client->setAuthConfig(OAUTH_CREDENTIALS);
    $client->setAccessType('offline');
    $client->setPrompt('select_account consent');
    if (USE_PROXY) {
        $httpClient = new GuzzleHttp\Client(['proxy' => PROXY]);
        $client->setHttpClient($httpClient);
    }
    // Load previously authorized token from a file, if it exists.
    // The file token.json stores the user's access and refresh tokens, and is
    // created automatically when the authorization flow completes for the first
    // time.
    $tokenPath = 'token.json';
    if (file_exists($tokenPath)) {
        $accessToken = json_decode(file_get_contents($tokenPath), true);
        $client->setAccessToken($accessToken);
    }

    // If there is no previous token or it's expired.
    if ($client->isAccessTokenExpired()) {
        // Refresh the token if possible, else fetch a new one.
        if ($client->getRefreshToken()) {
            $client->fetchAccessTokenWithRefreshToken($client->getRefreshToken());
        } else {
            // Request authorization from the user.
            $authUrl = $client->createAuthUrl();
            printf("Open the following link in your browser:\n%s\n", $authUrl);
            print 'Enter verification code: ';
            $authCode = trim(fgets(STDIN));

            // Exchange authorization code for an access token.
            $accessToken = $client->fetchAccessTokenWithAuthCode($authCode);
            $client->setAccessToken($accessToken);

            // Check to see if there was an error.
            if (array_key_exists('error', $accessToken)) {
                throw new Exception(join(', ', $accessToken));
            }
        }
        // Save the token to a file.
        if (!file_exists(dirname($tokenPath))) {
            mkdir(dirname($tokenPath), 0700, true);
        }
        file_put_contents($tokenPath, json_encode($client->getAccessToken()));
    }
    return $client;
}
