# Coding Guidelines for Dryer Project

## Important Rules

### 1. Comments Language
**All code comments MUST be written in English**
- Function documentation
- Inline comments
- Header comments
- Documentation blocks

Example:
```cpp
// Good: Calculate the average temperature
// Bad: Calculer la température moyenne
```

### 2. Logging Migration
**When adding or modifying code that contains Serial.print() or Serial.println() statements:**
- Migrate them to use Logger.cpp
- Use appropriate log levels:
  - `Logger::Debug()` - For detailed debugging information
  - `Logger::Info()` - For informational messages
  - `Logger::Warning()` - For warning conditions
  - `Logger::Error()` - For error conditions

Example:
```cpp
// Instead of:
Serial.println("Temperature sensor initialized");

// Use:
Logger::Info("Temperature sensor initialized");
```

### 3. Indentation and Brace Style
**2-space indentation. Allman brace style (opening brace on its own line).**
- Access specifiers (`public:`, `private:`, `protected:`) are not indented inside the class body
- Do NOT use K&R style (brace on same line) or 4-space indentation

Example:
```cpp
// Good
class Foo
{
public:
  void Bar();

private:
  int value_;
};

void Foo::Bar()
{
  if (value_ > 0)
  {
    value_--;
  }
}

// Bad — K&R style with 4-space indent
class Foo {
 public:
    void Bar();
};
```

### 4. Additional Best Practices
- Keep functions focused and single-purpose
- Use meaningful variable and function names
- Follow existing code style and naming conventions
- Test thoroughly after changes
