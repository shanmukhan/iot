import machine
from machine import Pin
import time
import camera
import ubinascii
import socket
import _thread as th
from umqttsimple import MQTTClient



# (host, port) = ('13.235.33.131', 5000)

MQTT_SERVER = '54.73.92.158' #'192799ffef1a4066a37a6ce3ea09d8bf.s1.eu.hivemq.cloud'
MQTT_PORT = 8883
MQTT_SUB_TOPIC = "/shanmukhan/iot/esp32/onoff"
MQTT_PUB_TOPIC = "/shanmukhan/iot/esp32/stream"
MQTT_CLIENT_ID = 'simple_id_rnd'
MQTT_USERNAME = b'shanmukhan'
MQTT_PASSWORD = b'Iotuser1'


MQTT_CLIENT = None

(WEBSOCKET_HOST, WEBSOCKET_PORT) = ('34.100.132.136', 15555)  # websocket conn

(REST_SERVICE_HOST, REST_SERVICE_PORT) = ('34.100.132.136', 11234)

LOCAL_SERVER_IP = '192.168.1.35'
LOCAL_SERVER_SOCK_PORT = 82

LIVE_STREAM = False
CAPTURE_IMAGE = False


def _post_headers(host, port, url, ctype='json', size=5):
    header = ( b"POST {url} HTTP/1.1\r\nContent-Type: application/{ctype}\r\nContent-Length: {size}\r\nHost: {host}:{port}\r\nConnection: close\r\n" )

    header = header.format(host=host, port=port, url=url, ctype=ctype, size=size)

    #if encode:
    #    header = header.encode('iso-8859-1')

    return header


def send(host, port, payload, retry_count=3):

    socket_ = None
    for i in range(retry_count):
        try:
            socket_ = socket.socket()
            socket_.connect((host, port))
            socket_.send(payload)
            print('SENT payload')
            break

        except Exception as e:
            print('Retry', i, e)
            time.sleep(1)
    if socket_ and hasattr(socket_, 'close'):
        socket_.close()


def post(url, data):
    global REST_SERVICE_PORT, REST_SERVICE_PORT

    data = ubinascii.b2a_base64(data)

    headers = _post_headers(REST_SERVICE_HOST, REST_SERVICE_PORT, url, size=len(data))
    payload = headers + "\r\n" + data

    send(REST_SERVICE_HOST, REST_SERVICE_PORT, payload)



def init_camera():
    import camera

    camera.init()
    camera.quality(10)  # initially it was 12, 6
    camera.framesize(8)  # initially it was 9, 4
    print("Camera initialized")


def wifi():
    import network

    wlan = network.WLAN(network.STA_IF) # create station interface
    wlan.active(True)       # activate the interface
    wlan.scan()             # scan for access points
    print('Is connected to wifi? ', wlan.isconnected())      # check if the station is connected to an AP
    #wlan.connect('NETGEAR35', 'silentvase911') # connect to an AP
    wlan.connect('BSRSP', 'BSRSP$126') # connect to an AP
    wlan.config('mac')      # get the interface's MAC address
    print('IP Config is', wlan.ifconfig())         # get the interface's IP/netmask/gw/DNS addresses
    print('Is connected2 to wifi? ', wlan.isconnected())
    
    while not wlan.isconnected():
        pass
    

def wifi_static():
    import network

    wlan = network.WLAN(network.STA_IF) # create station interface
    #wlan.init(WLAN.STA)
    # address, network mask, gateway address, dns server address
    wlan.ifconfig(('192.168.1.35', '255.255.255.0', '192.168.1.1', '8.8.8.8'))
    
    wlan.active(True)       # activate the interface
    wlan.scan()             # scan for access points
    print('Is connected to wifi? ', wlan.isconnected())      # check if the station is connected to an AP
    wlan.connect('NETGEAR35', 'silentvase911') # connect to an AP
    # wlan.connect('BSRSP', 'BSRSP$126') # connect to an AP
    wlan.config('mac')      # get the interface's MAC address
    print('IP Config is', wlan.ifconfig())         # get the interface's IP/netmask/gw/DNS addresses
    print('Is connected2 to wifi? ', wlan.isconnected())
    
    while not wlan.isconnected():
        pass
    
    print('*************** IP Config is', wlan.ifconfig())         # get the interface's IP/netmask/gw/DNS addresses
    

def access_point():
    ap = network.WLAN(network.AP_IF) # create access-point interface
    ap.config(essid='ESP-AP') # set the ESSID of the access point
    ap.config(max_clients=10) # set how many clients can connect to the network
    ap.active(True)         # activate the interface
    print("Access point setup is complete")
    
    
def start_server_socket(host, port):
    print("port is", port)
    port = port
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((host, port))
    s.listen(5)
    
    print('server started')
    global LIVE_STREAM
    
    while True:
        conn, addr = s.accept()
        conn.settimeout(3.0)
        print('Got a connection from', addr)
        request = conn.recv(50)
        print("Request is", request)
        
        LIVE_STREAM = eval(request) if request else False
        
        conn.close()
        
        
        print('LIVE_STREAM is', LIVE_STREAM)
        

def mqtt_sub_cb(topic, msg, retain=False, dup=False):
  print('Callback from mqtt:', (topic, msg))
  global LIVE_STREAM
  LIVE_STREAM = eval(msg) if msg else None
  

def mqtt_connect_and_subscribe():
  
  # client = MQTTClient(MQTT_CLIENT_ID, MQTT_SERVER, 1883, MQTT_USERNAME, MQTT_PASSWORD, ssl=False)
  
  global MQTT_CLIENT
  
  MQTT_CLIENT = MQTTClient(MQTT_CLIENT_ID, 'broker.hivemq.com', 1883)
  
  MQTT_CLIENT.set_callback(mqtt_sub_cb)
  MQTT_CLIENT.connect()
  MQTT_CLIENT.subscribe(MQTT_SUB_TOPIC)
  print('Connected to MQTT broker')
  return MQTT_CLIENT


def restart_and_reconnect():
  print('Failed to connect to MQTT broker. Reconnecting...')
  time.sleep(10)
  machine.reset()


#time.sleep(4)

#wifi_static()
wifi()

#time.sleep(4)

init_camera()

#time.sleep(5)

try:
    client = mqtt_connect_and_subscribe()
    print('mqtt client is', client)
except OSError as e:
    print('******')
    print(e)
    # restart_and_reconnect()
    print(e)

#th.start_new_thread(start_server_socket, (LOCAL_SERVER_IP, LOCAL_SERVER_SOCK_PORT))


def ws():
    #socket_ = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socket_ = socket.socket()
    socket_.connect((WEBSOCKET_HOST, WEBSOCKET_PORT))
    global LIVE_STREAM
    
    if not LIVE_STREAM:
        return
    
    for i in range(99000):
        try:
            data = camera.capture()
            print('.')
            data = ubinascii.b2a_base64(data)
            socket_.send(data)
            print(i, 'SENT image', len(data))
            time.sleep(0.2)
            
            
            global LIVE_STREAM
            if not LIVE_STREAM:
                print('Break on live stream')
                break

        except Exception as e:
            print('Retry', i, e)
            time.sleep(1)
        #if i == 9:
        #    socket_.send('close')
            
    if socket_ and hasattr(socket_, 'close'):
        print('closing socket')
        socket_.close()


def mqtt_stream():
    
    print('turning on stream')
    
    global MQTT_CLIENT
    global LIVE_STREAM
    
    if not LIVE_STREAM:
        return
    
    for i in range(99000):
        try:
            data = camera.capture()
            print('.')
            data = ubinascii.b2a_base64(data)
            
            MQTT_CLIENT.publish(MQTT_PUB_TOPIC, data)
            
            print(i, 'SENT image', len(data))
            #time.sleep(0.2)
            
            
            global LIVE_STREAM
            if not LIVE_STREAM:
                print('Break on live stream')
                break

        except Exception as e:
            print('Retry', i, e)
            time.sleep(1)
        #if i == 9:
        #    socket_.send('close')
            
    
# ws()

motion = False

def handle_interrupt(pin):
     global motion
     motion = True
     global interrupt_pin
     interrupt_pin = pin


led = Pin(4, Pin.OUT)

# post('/register', '')

print("Starting while loop")

while True:
    if LIVE_STREAM:
        #ws()
        mqtt_stream()
    elif CAPTURE_IMAGE:
        buf = camera.capture()
        post('/images', buf)
        CAPTURE_IMAGE = False
        
    #print('check msg')
    if LIVE_STREAM:
        time.sleep(5)
    else:
        time.sleep(1)
        
    MQTT_CLIENT.check_msg()  # this is non-blocking, use wait_msg() for blocking
    
    #print('after check msg')
        

# pir = Pin(14, Pin.IN)
# 
# pir.irq(trigger=Pin.IRQ_RISING, handler=handle_interrupt)
# 
# while True:
#     if motion:
#         print('Motion detected! Interrupt caused by:', interrupt_pin)
#         led.value(1)
#         buf = camera.capture()
#         post('/images', buf)
#         led.value(0)
#         print('Motion stopped!')
#         motion = False
#     else:
#         print('nooooo')
#         #led.value(1)
#         time.sleep(0.5)
#         #led.value(0)
#         #sleep(3)
#