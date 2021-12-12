import React, { useState, useEffect } from "react";
import { w3cwebsocket as W3CWebSocket } from "websocket";

import ImageStore from "./ImageStore";
// import webserver from "./WebsocketServer";

// webserver();

// const client = new W3CWebSocket("ws://localhost:8000");

// Base64 string data

const sample_data = "";

function Image(props) {
  const [data, setData] = useState(props.data || sample_data);
  ImageStore.subscribe(setData.bind(setData));

  console.log("rendering");

  useEffect(() => {
    const client = new W3CWebSocket("ws://localhost:8000");

    client.onopen = () => {
      console.log("WebSocket Client Connected");
      client.send("hi");
    };

    client.onmessage = (message) => {
      console.log(
        "WS msg is: " +
          JSON.stringify(message, [
            "message",
            "arguments",
            "type",
            "name",
            "data",
          ])
      );
      console.log("data is: " + message.data);

      setData(message.data);
      // console.log("Message is:" + JSON.stringify(Object.keys(message)));
      // const dataFromServer = JSON.parse(message.data);
    };

    client.onclose = (msg) => {
      console.log(
        "Disconnected." +
          JSON.stringify(msg, ["message", "arguments", "type", "name"])
      );
    };
  }, []);

  return <img src={`data:image/jpeg;base64,${data}`} />;
}

export default Image;
