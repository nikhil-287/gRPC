import grpc
import data_pb2 as data_pb2
import data_pb2_grpc as data_pb2_grpc
import time
import random

def run():
    channel = grpc.insecure_channel('localhost:50051')  # Node A's address
    stub = data_pb2_grpc.DataServiceStub(channel)

    events = [
        "login", "purchase", "signup", "logout", "update",
        "delete", "reset_password", "view", "click", "add_to_cart"
    ]

    num_messages = int(input("Enter number of messages to send: "))

    for i in range(num_messages):
        user_id = f"{100 + i:03}"
        event = random.choice(events)
        payload = f"userID:{user_id},event:{event}"

        request = data_pb2.DataRequest(payload=payload)
        stub.SendData(request)
        print(f"Sent: {payload}")
        time.sleep(0.2)  # adjustable delay between messages

if __name__ == '__main__':
    run()
