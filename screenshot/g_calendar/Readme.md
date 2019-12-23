### Google calendar reader (Oauth)

This is a very basic and easy example to have a read only access to your Google calendar using PHP.

1. To authorize it go first to:

https://developers.google.com/calendar/quickstart/php

2. Click on **Enable the calendar API**

And download credentials.json

3. This credentials.json should be placed outside the Web public directory and referenced in the index.php file on:

define("OAUTH_CREDENTIALS", "/home/myuser/credentials.json");

That's it, when you access index.php from command line, you should be able to generate a Oauth token that is saved locally.

### Customize the design

Just edit the basic HTML files in template folder and add any design / CSS / Logo you want.

The base design is on:

template.html

And for every calendar item row there is a second file:

partial_event_row.html

Editing this two files and using twitter bootstrap is very easy to modify the layout of your calendar.

Preview:
![Calendar preview](https://raw.githubusercontent.com/martinberlin/eink-calendar/master/screenshot/preview/640x384.bmp)
