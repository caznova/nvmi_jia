collect powerusage,temperature,devicename,fanspeed (nvidia gpu) in machine and publish to specific url 

example (http get) 
https://dev.xxx.in.th/test/watt.php?t=1&pc=RIG1&milwatt=8270&devices=GeForce%20GTX%201080,GeForce%20GTX%201080&temps=42,42&fans=28,28&uptime=1012


milwatt = sum (miliwatts)
devices = devname1,devname2
temps = dev1temp,dev2temp (Celcius)
fans = dev1fanspeed,dev2fanspeed (%)
