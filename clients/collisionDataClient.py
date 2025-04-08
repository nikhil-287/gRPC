import grpc
import threading
from data_pb2 import DataRequest, Empty
from data_pb2_grpc import DataServiceStub

def send_chunk_from_file(filename, stub):
    with open(filename, 'r') as f:
        for line in f:
            payload = line.strip()
            if not payload:
                continue
            request = DataRequest(payload=payload)
            try:
                response = stub.SendData(request)
            except grpc.RpcError as e:
                print(f"❌ Error sending payload: {e.details()}")

def main():
    filenames = [
        "client1_data.txt",
        "client2_data.txt",
        "client3_data.txt"
    ]

    # Connect to Node A
    channel = grpc.insecure_channel('localhost:50051')
    stub = DataServiceStub(channel)

    # Create one thread per file
    threads = []
    for file in filenames:
        t = threading.Thread(target=send_chunk_from_file, args=(file, stub))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    print("✅ All records sent.")

if __name__ == "__main__":
    main()
