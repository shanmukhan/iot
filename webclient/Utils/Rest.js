import axios from "axios";

class RestUtils {
  static withCredentials() {
    if (1 < 0) {
      return {
        config: {
          // headers: { "Content-Type": "application/x-www-form-urlencoded" },
          headers: {
            // "Content-Type": "multipart/form-data",
            "Acces-Control-Allow-Origin": "*",
          },
          // credentials: "same-origin",
        },
        withCredentials: true,
      };
    } else {
      return {};
    }
  }

  static api = axios.create({ baseURL: "http://34.100.132.136:11234" });

  static get(url, callback) {
    console.log("GET: " + url);

    this.api.get(url, RestUtils.withCredentials()).then((res) => {
      console.log("GET output:" + this.stringify(res.data));
      callback(res.data);
    });
  }

  static put(url, data, callback) {
    console.log("PUT: " + url + " :: " + this.stringify(data));

    this.api.put(url, data, RestUtils.withCredentials()).then((res) => {
      console.log("PUT output: " + this.stringify(res.data));
      callback(res.data);
    });
  }

  static post(url, data, callback) {
    console.log("POST: " + url + " :: " + this.stringify(data));

    this.api.post(url, data, RestUtils.withCredentials()).then((res) => {
      console.log("POST output: " + this.stringify(res.data));
      callback(res.data);
    });
  }

  static delete(url, data, callback) {
    console.log("DELETE: " + url + " :: " + this.stringify(data));

    this.api.delete(url, data, RestUtils.withCredentials()).then((res) => {
      console.log("DELETE output: " + this.stringify(res.data));
      callback(res.data);
    });
  }

  static login(url, data, callback) {
    this.api
      .post(url, null, {
        params: {
          username: data.username,
          password: data.password,
        },
        config: {
          // headers: { "Content-Type": "application/x-www-form-urlencoded" },
          headers: {
            "Content-Type": "multipart/form-data",
            "Acces-Control-Allow-Origin": "*",
          },
          credentials: "same-origin",
        },
        withCredentials: RestUtils.includeCredentials(),
      })
      .then((res) => {
        console.log("Post res: " + this.stringify(res));
        callback(res);
      });
  }

  static login2(url, data, callback) {
    console.log("Hello");
    fetch("http://localhost:8080/login", {
      method: "POST",
      body: new URLSearchParams(data),
      // credentials: "include",
      headers: {
        "Content-Type": "application/json",
      },
    })
      .then((res) => {
        console.log("Fetch: " + this.stringify(res));
        console.log(data);
        console.log(
          "Cookie: " +
            JSON.stringify(data) +
            "::" +
            JSON.stringify(document.cookie)
        );
      })
      .catch((error) => {
        console.log(error);
        console.log("err: " + JSON.stringify(error));
      });
  }

  static stringify(data) {
    if (data === {}) {
      return data;
    }
    if (Array.isArray(data)) {
      return JSON.stringify(data.map((e) => [e.id, e.status]));
    } else {
      return JSON.stringify([data.id, data.status]);
    }
  }
}

export default RestUtils;
