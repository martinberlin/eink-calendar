## E-ink calendar

A very easy and straight-forward Eink calendar. 
At the moment just reading events from google calendar, rendering them to a simmple webpage, and then reading it from Espressif chip as a screenshot with BMP format.

### Our approach to make ań easy E-Ink calendar

- An endpoint on the web that prints a webpage with the contents you need displayed on Eink
- A simple screenshot to BMP endpoint
- The calendar will wake up every morning and read this screenshot

There is a PHP example to do this in the directory /screenshot