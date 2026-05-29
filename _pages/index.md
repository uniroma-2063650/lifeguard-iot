---
title: Index
permalink: /
---

## My Blog Posts

<ul>
  {% for post in site.posts %}
  <li><a href="{{ post.url | relative_url }}" class="post-preview">{{ post.date | date_to_string }}: {{ post.title }}</a></li>
  {% endfor %}
</ul>
