from flask import Flask, render_template
from flask import request
from flask import jsonify
import base64
import socket


# from flask_restful import Resource, Api

app = Flask(__name__)
# api = Api(app)

FLASK_PORT = 11234

ESP32_SERVER_SOCK_HOST = None
ESP32_SERVER_SOCK_PORT = 82


@app.route('/', methods=['GET'])
def home():
    return "Service is Up!"

@app.route('/health', methods=['GET'])
def health():
    return "Success"


@app.route('/register', methods=['POST'])
def register():
    print('Request is coming from IP:', request.remote_addr)
    global ESP32_SERVER_SOCK_HOST
    ESP32_SERVER_SOCK_HOST = request.remote_addr
    return jsonify({'ip': request.remote_addr}), 200


@app.route('/streamon', methods=['GET'])
def stream_on():
    print('Request is coming from IP:', request.remote_addr)
    try:
        socket_ = socket.socket()
        socket_.connect((ESP32_SERVER_SOCK_HOST, ESP32_SERVER_SOCK_PORT))

        out = socket_.sendall(bytes("True", 'utf8'))
        print('data sent to server sock', out)
        #socket_.close()
    except Exception as e:
        print(e)

    response = jsonify({'status': 'success'})
    response.headers.add('Access-Control-Allow-Origin', '*')

    return response, 200


@app.route('/streamoff', methods=['GET'])
def stream_off():
    print('Request is coming from IP:', request.remote_addr)
    try:
        socket_ = socket.socket()
        socket_.connect((ESP32_SERVER_SOCK_HOST, ESP32_SERVER_SOCK_PORT))

        out = socket_.sendall(bytes("False", 'utf8'))
        print('data sent to server sock', out)
        socket_.close()
    except Exception as e:
        print(e)

    response = jsonify({'status': 'success'})
    response.headers.add('Access-Control-Allow-Origin', '*')

    return response, 200



@app.route("/images", methods=['POST'])
def new_image():
        
    data = base64.decodebytes(request.data)

    # print("Data is:", data, request)    
    # print("Headers are:",request.headers)
    #print("Body is:", dir(request))
    # print('Form:', request.form)
    # print(request.args, request.values)

    with open("/home/ec2-user/iot/static/latest_image.jpg", 'wb') as f1:
        f1.write(data)

    return "Image Stored."

@app.route('/latest_image')
def latest_img():
    path = '/static/latest_image.jpg'
    return render_template('index.html', user_image=path)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=True)
    # app.run(debug=True)
