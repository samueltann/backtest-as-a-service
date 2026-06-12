# Build from the repository root:  docker build -f backend/Dockerfile .
# Stage 1: compile the C++ engine
FROM gcc:13 AS engine-build
RUN apt-get update && apt-get install -y --no-install-recommends cmake && rm -rf /var/lib/apt/lists/*
COPY engine /src/engine
RUN cmake -S /src/engine -B /build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build -j"$(nproc)" --target backtest-engine

# Stage 2: build the Spring Boot jar
FROM eclipse-temurin:17-jdk AS backend-build
WORKDIR /src
COPY backend/mvnw .
COPY backend/.mvn .mvn
COPY backend/pom.xml .
RUN ./mvnw -q -B dependency:go-offline
COPY backend/src src
RUN ./mvnw -q -B package -DskipTests

# Stage 3: slim runtime with the engine binary, the jar, and the seed data
FROM eclipse-temurin:17-jre
WORKDIR /app
COPY --from=engine-build /build/backtest-engine /app/engine/backtest-engine
COPY --from=backend-build /src/target/*.jar /app/app.jar
COPY data/csv /app/data/csv

ENV ENGINE_BINARY_PATH=/app/engine/backtest-engine \
    ENGINE_BUNDLED_CSV_DIR=/app/data/csv \
    ENGINE_CACHE_DIR=/app/data/engine-cache \
    ENGINE_WORK_DIR=/tmp/backtests

EXPOSE 8080
ENTRYPOINT ["java", "-jar", "/app/app.jar"]
