import grpc
import threading
import json
from data_pb2 import DataRequest, Empty
from data_pb2_grpc import DataServiceStub

def load_port_from_config():
    with open('../routing.json', 'r') as f:
        config = json.load(f)
        address = config['address_map']['A']  # Correct path
        return address.split(':')[-1]  # Extract port (e.g., '50057')

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

    port = load_port_from_config()
    channel = grpc.insecure_channel(f'localhost:{port}')
    stub = DataServiceStub(channel)

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
