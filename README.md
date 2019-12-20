## E-ink calendar

A very easy and straight-forward Eink calendar. 
At the moment just has 3 simple options:

1. Renders a screenshot of a webpage (Calendar if you point it to a calendar page)
2. Renders a free title + text
3. Cleans screen

UX Preview when Espressif is online
![UX Preview](screenshot/calendar.local.png)

### Our approach to make an easy E-Ink calendar

- An endpoint on the web that prints a webpage with the contents you need displayed on Eink
- A simple screenshot to BMP endpoint
- The calendar will wake up every morning and read this screenshot

There is a PHP example to do this in the directory /screenshot

### Simple configuration

Just rename lib/Config/Config.h.dist to COnfig.h
and fill it with your WiFi name and password

Prepare any page that will render your calendar from any source (Google/Exchange) 
If you need an example just ask and I will upload my google Calendar view example.

http://twitter.com/martinfasani