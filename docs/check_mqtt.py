# Diagnostico: escucha los topics del reloj en el broker de Home Assistant.
# Uso: python docs/check_mqtt.py   (pide usuario y contrasena del broker)
import getpass, time, paho.mqtt.client as m
user = input("Usuario MQTT [reloj]: ") or "reloj"
pwd = getpass.getpass("Contrasena MQTT: ")
c = m.Client(m.CallbackAPIVersion.VERSION2, client_id="diag_pc")
c.username_pw_set(user, pwd)
c.on_connect = lambda c,u,f,rc,p: (print("Conexion al broker:", rc), c.subscribe([("reloj/#",0),("homeassistant/+/reloj_neopixel/config",0)]))
c.on_message = lambda c,u,msg: print(("[retenido] " if msg.retain else "[en vivo]  ") + msg.topic, "->", msg.payload[:120].decode(errors="replace"))
c.connect("192.168.1.4", 1883, 10); c.loop_start(); time.sleep(40); c.loop_stop()
print("Fin (40 s).")
