const ImageStore = {
  data: [],
  subscribersNames: [],
  subscribers: [],
  clear() {
    this.data = [];
  },

  init(msg) {
    // Array.prototype.push.apply(this.data, msg); // this.data.push(...msg);
    this.notify(msg);
  },

  /*
   *  Earlier this.data is storing all the data till current point.
   *  Now changing it - just pass the data and notify it to others, don't store the element.
   */
  add(msg) {
    // Array.prototype.push.apply(this.data, msg); // this.data.push(...msg);
    this.notify(msg);
  },
  subscribe(name, func) {
    // console.log("Subscribing: " + func);
    if (!this.subscribersNames.includes(name)) {
      this.subscribersNames.push(name);
      this.subscribers.push(func);
    }
  },
  notify(msg) {
    for (let idx = 0; idx < this.subscribers.length; idx++) {
      const subscriberFn = this.subscribers[idx];
      const subscriberNn = this.subscribersNames[idx];
      console.log(`Calling(sub-${subscriberNn}): ${subscriberFn.name}`);
      // subscriberFn(this.data);
      subscriberFn(msg);
    }
  },
};

export default ImageStore;
