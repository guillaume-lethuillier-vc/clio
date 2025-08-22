#!/bin/bash

echo "Starting Clio Hybrid Setup (Docker services + Local Clio)..."

# check if Docker is running
if ! docker info >/dev/null 2>&1; then
    echo "Docker is not running. Please start Docker first."
    exit 1
fi

# cleanup containers
echo "Cleaning up existing containers..."
docker compose -f compose-hybrid.yaml down 2>/dev/null || true

# start the ClickHouse and Rippled
docker compose -f compose-hybrid.yaml up -d

echo "Waiting for services to be ready..."

check_service_running() {
    local service=$1
    local max_attempts=5
    local attempt=1
    
            echo "checking if $service is running..."
    while [ $attempt -le $max_attempts ]; do
        if docker compose -f compose-hybrid.yaml ps | grep -q "$service.*Up"; then
            echo "$service is running"
            return 0
        fi
        echo "Waiting for $service to start... (attempt $attempt/$max_attempts)"
        sleep 5
        attempt=$((attempt + 1))
    done
    
    echo "$service failed to start"
    docker compose -f compose-hybrid.yaml logs --tail=20 "$service"
    return 1
}

check_service_health() {
    local service=$1
    local max_attempts=4
    local attempt=1
    
    echo "Checking $service connectivity..."
    while [ $attempt -le $max_attempts ]; do
        local healthy=false
        
        case $service in
            "clickhouse")
                # test ClickHouse HTTP endpoint (with authentication)
                if curl -sf --user "default:clickhouse" "http://localhost:8123/?query=SELECT%201" > /dev/null 2>&1; then
                    healthy=true
                fi
                ;;
            "rippled")
                # test rippled by checking if port is open (rippled can take longer to become fully ready)
                if nc -z localhost 50005 2>/dev/null; then
                    healthy=true
                fi
                ;;
        esac
        
        if [ "$healthy" = true ]; then
            echo "$service is healthy and responding"
            return 0
        fi
        
        echo "Waiting for $service... (attempt $attempt/$max_attempts)"
        sleep 10
        attempt=$((attempt + 1))
    done
    
    echo "$service failed to become healthy within expected time"
    docker compose -f compose-hybrid.yaml logs --tail=20 "$service"
    return 1
}

check_service_running "clickhouse" || exit 1
check_service_running "rippled" || exit 1

check_service_health "clickhouse" || exit 1
check_service_health "rippled" || exit 1


docker compose -f compose-hybrid.yaml ps

echo "   ClickHouse:    http://localhost:8123"
echo "   rippled HTTP:  http://localhost:50005"
echo "   rippled WS:    ws://localhost:51233"


echo "Building Clio locally..."
cd ../..
if ! ./build-clio.sh; then
    echo "Failed to build Clio"
    exit 1
fi
cd docs/clickhouse

# start Clio
echo "Starting Clio server..."
echo "View Clio logs in real-time..."
echo "Press Ctrl+C to stop Clio (Docker services will continue running)"
echo ""

../../build/clio_server -c config-hybrid.json
