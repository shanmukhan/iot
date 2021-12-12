from flask import Flask, render_template
from flask import request
import base64

# from flask_restful import Resource, Api

app = Flask(__name__)
# api = Api(app)


@app.route('/', methods=['GET'])
def home():
    return "Service is Up!"

@app.route('/health', methods=['GET'])
def health():
    return "Success"


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
    app.run(host='0.0.0.0', debug=True)
    # app.run(debug=True)
