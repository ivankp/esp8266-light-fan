const $ = (p,...args) => {
  if (p===null) {
    const x = args.shift();
    p = document.createElement(x);
  } else if (p.constructor === String) {
    p = document.getElementById(p);
  }
  for (let x of args) {
    if (x.constructor === String) {
      p = p.appendChild( (p instanceof SVGElement || x==='svg')
        ? document.createElementNS('http://www.w3.org/2000/svg', x)
        : document.createElement(x)
      );
    } else if (x.nodeType === Node.ELEMENT_NODE) {
      p = p.appendChild(x);
    } else if (x.constructor === Array) {
      for (let c of x)
        p.classList.add(c);
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

const clone = x => x.parentElement.appendChild(x.cloneNode());

const all_inputs = { };

const toggle = (id,val) => {
  const input = all_inputs[id];
  if (input) {
    if (input.old = input.checked = val) input.classList.add('on');
    else input.classList.remove('on');
  } else { // reload if server responded with an unknows input id
    window.location.reload();
  }
};

document.addEventListener('DOMContentLoaded', () => {
  const div1 = $(document.body, 'div');

  for (const [section, id_name, one] of [
    // ['Thermostat',[['cool','Cool'],['heat','Heat']],true],
    ['Bedroom Ceiling',[['light','Light'],['fan','Fan']]]
  ]) {
    const f = $(div1,'fieldset');
    $(f,'legend').textContent = section;
    const t = $(f,'table');
    const inputs = [ ];
    for (const [id, name] of id_name) {
      if (id in all_inputs) {
        document.body.innerHTML = '';
        throw new Error(`Repeated input id "${id}"`);
      }
      const tr = $(t,'tr');
      $(tr,'td').textContent = name+':';
      const s = $(tr,'td','label',['switch']);
      const input = $(s,'input',{id, type:'checkbox', events: {
        change: function(){
          const q = new URLSearchParams();
          const modified = [ ];
          const restore = () => {
            for (const x of modified)
              x.checked = x.old;
          };
          const f = (input) => {
            q.set(input.id,input.checked?'1':'0');
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
          fetch('set?'+q.toString(),{ referrer: '' })
          .then(resp => resp.json())
          .then(resp => {
            console.log(resp);
            if ('error' in resp) {
              alert(resp.error);
              restore();
            } else {
              for (const [key,val] of Object.entries(resp))
                toggle(key,val);
              for (const x of modified)
                if (!(x.id in resp)) x.checked = x.old;
            }
          })
          .catch(e => {
            alert('Request failed');
            restore();
            throw e;
          });
        }
      }});
      all_inputs[id] = input;
      inputs.push(input);
      input.old = input.checked;
      $(s,'span',['slider']);
    }
  }

  const div2 = $(document.body, 'div');
  const wifi = $(div2, 'span', ['click'], { events: {
    click: () => {
    }
  }}, 'svg', ['wifi'], { viewBox: '-10.192 -14 20.385 16', height: '1em' });
  $(wifi, 'circle', { r: 2, stroke: 'none' });
  $(clone(
  $(clone(
  $(wifi, 'path', {
    fill: 'none', 'stroke-linecap': 'round', 'stroke-width': 2,
    d: 'M -3.536 -3.536 A 5 5 0 0 1 3.536 -3.536'
  })), {
    d: 'M -6.364 -6.364 A 9 9 0 0 1 6.364 -6.364'
  })), {
    d: 'M -9.192 -9.192 A 13 13 0 0 1 9.192 -9.192'
  });


  fetch('get',{ referrer: '' })
  .then(resp => resp.json())
  .then(resp => {
    console.log(resp);
    if ('error' in resp) {
      alert(resp.error);
    } else {
      for (const [key,val] of Object.entries(resp))
        toggle(key,val);
    }
  })
  .catch(e => {
    alert('Request failed');
    throw e;
  });
});
