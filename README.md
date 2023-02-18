<div align="center">
  <h3><a href="https://github.com/ezekielathome">
    ~ezekielathome/</a>dike – Δίκη ( justice )
  </h3>
source engine plugin to detect view angle manipulation
</div>

#
### introduction

Dike is a small proof of concept serverside anticheat for games built on the Source Engine. It works by verifying that an angle delta, or change in angle, produced by a client is actually possible for that client to produce.

Dike was originally intended to be a much larger project aimed at providing comprehensive anticheat protection for games built on the Source Engine. However, after much consideration, I decided to release a small proof of concept of the view model manipulation detection method instead.

### notes
Please be aware, that as it is i don't consider dike "production ready" as even though i tried to account for most edge cases, there are still plenty that could occour (acceleration, joystick, etc...) i might look into implementing them sometime, but as of right now that isn't a priority.
