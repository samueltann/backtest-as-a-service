package com.samueltan.backtest.common;

import com.samueltan.backtest.auth.AuthService.EmailTakenException;
import com.samueltan.backtest.auth.AuthService.InvalidCredentialsException;
import com.samueltan.backtest.backtest.BacktestService.CapacityExceededException;
import com.samueltan.backtest.backtest.BacktestService.JobNotFoundException;
import com.samueltan.backtest.market.MarketDataService.UnknownSymbolException;
import com.samueltan.backtest.strategy.StrategyCatalogService.InvalidStrategyException;
import java.util.stream.Collectors;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.ResponseStatus;
import org.springframework.web.bind.annotation.RestControllerAdvice;

/** Maps domain exceptions to a consistent {"error": "..."} body. */
@RestControllerAdvice
public class ApiExceptionHandler {

    private static final Logger log = LoggerFactory.getLogger(ApiExceptionHandler.class);

    public record ApiError(String error) {
    }

    @ExceptionHandler({InvalidStrategyException.class, UnknownSymbolException.class,
            IllegalArgumentException.class})
    @ResponseStatus(HttpStatus.BAD_REQUEST)
    public ApiError badRequest(RuntimeException e) {
        return new ApiError(e.getMessage());
    }

    @ExceptionHandler(MethodArgumentNotValidException.class)
    @ResponseStatus(HttpStatus.BAD_REQUEST)
    public ApiError validation(MethodArgumentNotValidException e) {
        String message = e.getBindingResult().getFieldErrors().stream()
                .map(f -> f.getField() + " " + f.getDefaultMessage())
                .sorted()
                .collect(Collectors.joining("; "));
        return new ApiError(message.isEmpty() ? "invalid request" : message);
    }

    @ExceptionHandler(EmailTakenException.class)
    @ResponseStatus(HttpStatus.CONFLICT)
    public ApiError conflict(EmailTakenException e) {
        return new ApiError(e.getMessage());
    }

    @ExceptionHandler(InvalidCredentialsException.class)
    @ResponseStatus(HttpStatus.UNAUTHORIZED)
    public ApiError unauthorized(InvalidCredentialsException e) {
        return new ApiError(e.getMessage());
    }

    @ExceptionHandler(JobNotFoundException.class)
    @ResponseStatus(HttpStatus.NOT_FOUND)
    public ApiError notFound(JobNotFoundException e) {
        return new ApiError(e.getMessage());
    }

    @ExceptionHandler(CapacityExceededException.class)
    @ResponseStatus(HttpStatus.TOO_MANY_REQUESTS)
    public ApiError capacity(CapacityExceededException e) {
        return new ApiError(e.getMessage());
    }

    @ExceptionHandler(Exception.class)
    @ResponseStatus(HttpStatus.INTERNAL_SERVER_ERROR)
    public ApiError internal(Exception e) {
        log.error("Unhandled exception", e);
        return new ApiError("internal server error");
    }
}
