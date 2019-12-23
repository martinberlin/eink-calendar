### Google calendar reader (Oauth)

This is a very basic and easy exaple to have read only access to your calendar using PHP.
To authorize it go first to:

https://developers.google.com/calendar/quickstart/php

Click on "Enable the calendar API"

And download credentials.json

This credentials.json should be placed outside the Web public directory and referenced in the index.php file on:

define("OAUTH_CREDENTIALS", "/home/myuser/credentials.json");

That's it, when you access index.php from command line, you should be able to generate a Oauth token that is saved locally.

### Customize the design

Just edit the basic HTML files in template folder and add any design / CSS / Logo you want.

