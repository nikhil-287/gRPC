import grpc
import data_pb2 as data_pb2
import data_pb2_grpc as data_pb2_grpc
import time

def run():
    channel = grpc.insecure_channel('localhost:50051')  # Node A's address
    stub = data_pb2_grpc.DataServiceStub(channel)

    messages = [
        "userID:001,event:login",
        "userID:002,event:purchase",
        "userID:003,event:signup"
    ]

    for msg in messages:
        request = data_pb2.DataRequest(payload=msg)
        stub.SendData(request)
        print(f"Sent: {msg}")
        time.sleep(1)  # simulate streaming

if __name__ == '__main__':
    run()
