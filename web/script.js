const $ = (p, ...args) => {
  if (p.constructor === String) {
    p = document.getElementById(p);
  }
  for (let x of args) {
    if (x.constructor === String) {
      p = p.appendChild( (p instanceof SVGElement || x==='svg')
        ? document.createElementNS('http://www.w3.org/2000/svg', x)
        : document.createElement(x)
      );
    // } else if (x.nodeType === Node.ELEMENT_NODE) {
    //   p.appendChild(x);
    } else if (x.constructor === Array) {
      p.classList.add(...x);
    } else if (x.constructor === Function) {
      x(p);
    } else if (x.constructor === Object) {
      for (const [key,val] of Object.entries(x)) {
        if (key==='style') {
          for (const [k,v] of Object.entries(val)) {
            if (v!==null) p.style[k] = v;
            else p.style.removeProperty(k);
          }
        } else if (key==='events') {
          for (const [k,v] of Object.entries(val)) {
            if (v!==null) p.addEventListener(k,v);
            else p.removeEventListener(k);
          }
        } else if (key==='text') {
          p.textContent = val;
        } else {
          if (val!==null) {
            if (p instanceof SVGElement)
              p.setAttributeNS(null,key,val);
            else
              p.setAttribute(key,val);
          } else {
            if (p instanceof SVGElement)
              p.removeAttributeNS(null,key);
            else
              p.removeAttribute(key);
          }
        }
      }
    }
  }
  return p;
};
const $$ = (...args) => p => $(p, ...args);

const fetch_json = async url => {
  try {
    const resp = await fetch(url, { referrer: '' });
    if (!resp.ok) throw new Error(`${resp.status} ${resp.statusText}`);
    return await resp.json();
  } catch (e) {
    alert(`${url}: ${e.message}`);
    throw e;
  }
};

const all_inputs = { };

const toggle = ([id, val]) => {
  const input = all_inputs[id];
  if (input) {
    if (input.old = input.checked = val) input.classList.add('on');
    else input.classList.remove('on');
  } else { // reload if server responded with an unknown input id
    window.location.reload();
  }
};

let overlay = null;
const rmOverlay = () => {
  overlay.remove();
  overlay = null;
  $('main').classList.remove('inactive');
};

document.addEventListener('keydown', e => {
  if (overlay !== null && e.key === 'Escape') rmOverlay();
});

document.addEventListener('DOMContentLoaded', () => {
  const div1 = $('main', 'div');

  $('main', 'div',
    'span', ['click'], { events: {
      click: () => {
        $('main', ['inactive']);
        $(document.body, 'div', x => overlay = x,
          $$('div', { id: 'overlay' }),
          'div', { id: 'prompt' }, 'form', { events: {
            submit: e => {
              e.preventDefault();

              const f = e.target.elements; // form elements
              const p = e.target.querySelector('p');
              p.textContent = '';

              const disable = b => { for (const x of f) x.disabled = b; };
              disable(true);

              fetch('/connect',{
                method: 'POST',
                referrer: '',
                body: `${f.ssid.value}\0${f.pass.value}\0`
              }).then(r => {
                disable(false);
                p.style.color = r.ok ? '#0A0' : '#A00';
                return r.text();
              }).then(t => {
                p.textContent = t;
              }).catch(e => {
                alert(`connect: ${e.message}`);
                disable(false);
                f.pass.select();
                throw e;
              });
            }
          }},
            $$('label',
              $$('span', { text: 'SSID:' }),
              'input', { type: 'text', name: 'ssid' }, x => x.focus(),
            ),
            $$('label',
              $$('span', { text: 'PASS:' }),
              'input', { type: 'password', name: 'pass' }
            ),
            $$('input', { type: 'submit', value: 'Connect' }),
            $$('p'),
            'div', ['close', 'click'], { text: 'x', events: { click: rmOverlay }}
        );
      }
    }}, 'svg', { id: 'wifi', viewBox: '-10.192 -14 20.385 16' },
      $$('circle', { r: 2 }),
      'path', {
        fill: 'none', 'stroke-linecap': 'round', 'stroke-width': 2,
        d: 'M-3.536-3.536a5 5 0 0 1 7.072 0M-6.364-6.364a9 9 0 0 1 12.728 0M-9.192-9.192a13 13 0 0 1 18.384 0'
      }
  );

  for (const [section, id_name, one] of [
    // ['Thermostat',[['cool','Cool'],['heat','Heat']],true],
    ['Bedroom Ceiling',[['light','Light'],['fan','Fan']]]
  ]) {
    const t = $(div1, 'fieldset', $$('legend', { text: section }), 'table');
    const inputs = [ ];
    for (const [id, name] of id_name) {
      $(t, 'tr',
        $$('td', { text: name+':' }),
        'td', 'label', ['switch'],
        $$('input', { id, type: 'checkbox', events: {
          change: function(){
            const q = new URLSearchParams();
            const modified = [ ];
            const restore = () => {
              for (const x of modified)
                x.checked = x.old;
            };
            const f = input => {
              q.set(input.id, input.checked ? '1' : '0');
              modified.push(input);
            };
            if (one) {
              for (const x of inputs) {
                if (x!==this) x.checked = false;
                f(x);
              }
            } else {
              f(this);
            }
            fetch_json('set?'+q.toString())
            .then(resp => {
              console.log(resp);
              Object.entries(resp).forEach(toggle);
              for (const x of modified)
                if (!(x.id in resp)) x.checked = x.old;
            })
            .catch(e => {
              restore();
              throw e;
            });
          }
        }}, input => {
          all_inputs[id] = input;
          inputs.push(input);
          input.old = input.checked;
        }),
        'span', ['slider']
      );
    }
  }

  fetch_json('get')
  .then(resp => {
    console.log(resp);
    Object.entries(resp).forEach(toggle);
  });
});
