package com.samueltan.backtest.auth;

import java.util.Locale;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;

@Service
public class AuthService {

    private final UserRepository users;
    private final PasswordEncoder passwordEncoder;
    private final JwtService jwtService;

    public AuthService(UserRepository users, PasswordEncoder passwordEncoder,
                       JwtService jwtService) {
        this.users = users;
        this.passwordEncoder = passwordEncoder;
        this.jwtService = jwtService;
    }

    public record AuthResult(String token, String email) {
    }

    public AuthResult register(String email, String password) {
        String normalized = email.strip().toLowerCase(Locale.ROOT);
        if (users.existsByEmail(normalized)) {
            throw new EmailTakenException();
        }
        User user = users.save(new User(normalized, passwordEncoder.encode(password)));
        return new AuthResult(jwtService.issue(user.getId(), user.getEmail()), user.getEmail());
    }

    public AuthResult login(String email, String password) {
        String normalized = email.strip().toLowerCase(Locale.ROOT);
        User user = users.findByEmail(normalized)
                .orElseThrow(InvalidCredentialsException::new);
        if (!passwordEncoder.matches(password, user.getPasswordHash())) {
            throw new InvalidCredentialsException();
        }
        return new AuthResult(jwtService.issue(user.getId(), user.getEmail()), user.getEmail());
    }

    public static class EmailTakenException extends RuntimeException {
        public EmailTakenException() {
            super("an account with this email already exists");
        }
    }

    public static class InvalidCredentialsException extends RuntimeException {
        public InvalidCredentialsException() {
            super("invalid email or password");
        }
    }
}
