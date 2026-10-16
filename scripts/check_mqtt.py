# Diagnostics: listens to the clock's topics on the Home Assistant broker.
# Usage: python scripts/check_mqtt.py [broker_host]   (asks for the broker user and password)
import getpass, sys, time, paho.mqtt.client as m
host = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.4"
user = input("MQTT user [reloj]: ") or "reloj"
pwd = getpass.getpass("MQTT password: ")
c = m.Client(m.CallbackAPIVersion.VERSION2, client_id="diag_pc")
c.username_pw_set(user, pwd)
c.on_connect = lambda c,u,f,rc,p: (print("Broker connection:", rc), c.subscribe([("reloj/#",0),("homeassistant/+/reloj_neopixel/config",0)]))
c.on_message = lambda c,u,msg: print(("[retained] " if msg.retain else "[live]     ") + msg.topic, "->", msg.payload[:120].decode(errors="replace"))
c.connect(host, 1883, 10); c.loop_start(); time.sleep(40); c.loop_stop()
print("Done (40 s).")
